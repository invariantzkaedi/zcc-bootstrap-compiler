/// ZCC Rust SHM Ring Buffer — cross-platform zero-copy Rust→ZCC bridge.
///
/// Protocol: SPSC lock-free ring of `MAX_SLOTS` fixed slots (each WIRE_SLOT_SIZE bytes).
/// Rust is the sole writer (head), ZCC is the sole reader (tail).
/// Both sides see the same flat binary `WireInitNode` tree — no deserialization.

use std::sync::atomic::{AtomicU32, AtomicU64, Ordering};

// ── Wire protocol constants (must mirror zcc_ast_bridge.h) ──────────────────
pub const WIRE_INIT_MAGIC: u32 = 0x5A434349; // "ZCCI"
pub const SHM_RING_SIZE: usize = 16 * 1024 * 1024; // 16 MiB
pub const MAX_SLOTS: usize = 256;
pub const WIRE_SLOT_SIZE: usize = SHM_RING_SIZE / MAX_SLOTS; // 64 KiB per slot

pub const WIRE_LIST: u8 = 1;
pub const WIRE_VALUE: u8 = 2;
pub const WIRE_DESIGNATED_FIELD: u8 = 3;
pub const WIRE_DESIGNATED_INDEX: u8 = 4;

// ── Wire node layout (repr(C) must match zcc_ast_bridge.h exactly) ──────────

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WireInitNode {
    pub magic: u32,
    pub kind: u8,
    pub payload_size: u32,
    pub u: WireUnion,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union WireUnion {
    pub list: WireList,
    pub value: WireValue,
    pub designated: WireDesignated,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WireList {
    pub child_count: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WireValue {
    pub vkind: u8, // 0=i64, 1=f64, 2=str, 3=ident
    pub len: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WireDesignated {
    pub dkind: u8, // 0=field, 1=index
    pub name_len: u32,
    pub index: i64,
}

// ── Builder (ergonomic Rust-side tree construction) ──────────────────────────

pub struct WireInitNodeBuilder {
    pub kind: u8,
    pub children: Vec<WireInitNodeBuilder>,
    pub value_bytes: Vec<u8>,
    pub vkind: u8,
    pub des_kind: u8,
    pub name: String,
    pub index: i64,
}

impl WireInitNodeBuilder {
    pub fn new_list() -> Self {
        Self {
            kind: WIRE_LIST,
            children: vec![],
            value_bytes: vec![],
            vkind: 0,
            des_kind: 0,
            name: String::new(),
            index: 0,
        }
    }

    pub fn new_value_i64(val: i64) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_VALUE;
        b.vkind = 0;
        b.value_bytes = val.to_le_bytes().to_vec();
        b
    }

    pub fn new_value_f64(val: f64) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_VALUE;
        b.vkind = 1;
        b.value_bytes = val.to_le_bytes().to_vec();
        b
    }

    pub fn new_value_str(s: &str) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_VALUE;
        b.vkind = 2;
        // null-terminated for ZCC's cc_strdup
        let mut bytes = s.as_bytes().to_vec();
        bytes.push(0);
        b.value_bytes = bytes;
        b
    }

    pub fn new_value_ident(name: &str) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_VALUE;
        b.vkind = 3;
        let mut bytes = name.as_bytes().to_vec();
        bytes.push(0);
        b.value_bytes = bytes;
        b
    }

    pub fn new_designated_field(name: &str, init: WireInitNodeBuilder) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_DESIGNATED_FIELD;
        b.name = name.to_string();
        b.children.push(init);
        b
    }

    pub fn new_designated_index(idx: i64, init: WireInitNodeBuilder) -> Self {
        let mut b = Self::new_list();
        b.kind = WIRE_DESIGNATED_INDEX;
        b.index = idx;
        b.children.push(init);
        b
    }

    pub fn append(&mut self, child: WireInitNodeBuilder) {
        if self.kind == WIRE_LIST {
            self.children.push(child);
        }
    }

    /// Serialize into `buf` starting at `*cursor`. Returns `true` on success.
    pub fn serialize(&self, buf: &mut [u8], cursor: &mut usize) -> bool {
        let hdr_size = std::mem::size_of::<WireInitNode>();
        if *cursor + hdr_size > buf.len() {
            return false;
        }

        let start = *cursor;
        // Safety: buf is valid memory, cursor is bounds-checked above.
        let header = unsafe { &mut *(buf.as_mut_ptr().add(start) as *mut WireInitNode) };
        header.magic = WIRE_INIT_MAGIC;
        header.kind = self.kind;
        *cursor += hdr_size;

        match self.kind {
            WIRE_LIST => {
                header.u.list.child_count = self.children.len() as u32;
                for child in &self.children {
                    if !child.serialize(buf, cursor) {
                        return false;
                    }
                }
            }

            WIRE_VALUE => {
                header.u.value.vkind = self.vkind;
                header.u.value.len = self.value_bytes.len() as u32;
                let end = *cursor + self.value_bytes.len();
                if end > buf.len() {
                    return false;
                }
                buf[*cursor..end].copy_from_slice(&self.value_bytes);
                *cursor = end;
            }

            WIRE_DESIGNATED_FIELD => {
                header.u.designated.dkind = 0;
                header.u.designated.name_len = self.name.len() as u32;
                let nb = self.name.as_bytes();
                let end = *cursor + nb.len();
                if end > buf.len() {
                    return false;
                }
                buf[*cursor..end].copy_from_slice(nb);
                *cursor = end;
                if self.children.is_empty() || !self.children[0].serialize(buf, cursor) {
                    return false;
                }
            }

            WIRE_DESIGNATED_INDEX => {
                header.u.designated.dkind = 1;
                header.u.designated.index = self.index;
                if self.children.is_empty() || !self.children[0].serialize(buf, cursor) {
                    return false;
                }
            }

            _ => {}
        }

        let total = *cursor - start;
        // Write payload_size back into header now that we know it.
        let hdr = unsafe { &mut *(buf.as_mut_ptr().add(start) as *mut WireInitNode) };
        hdr.payload_size = total as u32;
        true
    }
}

// ── Shared memory ring buffer layout ────────────────────────────────────────

#[repr(C)]
pub struct ShmRingBuffer {
    pub head: AtomicU64, // writer cursor (Rust)
    pub tail: AtomicU64, // reader cursor (ZCC)
    pub drop_count: AtomicU32,
    pub data: [u8; SHM_RING_SIZE],
}

// ── Platform SHM open ───────────────────────────────────────────────────────

/// RAII wrapper that owns the OS handle and the mapped view.
pub struct ShmRing {
    pub ring: *mut ShmRingBuffer,
    // Keep the platform handle alive for the process lifetime.
    #[cfg(unix)]
    _mmap: memmap2::MmapMut,
    #[cfg(windows)]
    _handle: windows_handle::OwnedHandle,
}

// Safety: ShmRingBuffer uses atomics; we only ever have one writer and one reader.
unsafe impl Send for ShmRing {}
unsafe impl Sync for ShmRing {}

impl ShmRing {
    pub fn create() -> Result<Self, String> {
        #[cfg(unix)]
        {
            use libc::{shm_open, ftruncate, O_RDWR, O_CREAT};
            let fd = unsafe {
                shm_open(
                    b"/zcc_ast_ring\0".as_ptr() as *const libc::c_char,
                    O_RDWR | O_CREAT,
                    0o666,
                )
            };
            if fd < 0 {
                return Err(format!(
                    "shm_open failed: {}",
                    std::io::Error::last_os_error()
                ));
            }
            let size = std::mem::size_of::<ShmRingBuffer>();
            let ret = unsafe { ftruncate(fd, size as libc::off_t) };
            if ret != 0 {
                return Err(format!(
                    "ftruncate failed: {}",
                    std::io::Error::last_os_error()
                ));
            }
            // Safety: fd is valid and size matches the struct.
            let mmap = unsafe { memmap2::MmapMut::map_mut(fd) }
                .map_err(|e| format!("mmap failed: {e}"))?;
            let ring = mmap.as_ptr() as *mut ShmRingBuffer;
            // Initialize the ring — safe because we just created the segment.
            unsafe {
                (*ring).head.store(0, Ordering::Release);
                (*ring).tail.store(0, Ordering::Release);
                (*ring).drop_count.store(0, Ordering::Release);
            }
            Ok(ShmRing { ring, _mmap: mmap })
        }

        #[cfg(windows)]
        {
            use std::ffi::OsStr;
            use std::iter::once;
            use std::os::windows::ffi::OsStrExt;
            use windows::Win32::System::Memory::{
                CreateFileMappingW, MapViewOfFile, FILE_MAP_ALL_ACCESS, PAGE_READWRITE,
            };
            use windows::Win32::Foundation::{CloseHandle, INVALID_HANDLE_VALUE};

            let name: Vec<u16> = OsStr::new("zcc_ast_ring").encode_wide().chain(once(0)).collect();
            let size = std::mem::size_of::<ShmRingBuffer>() as u32;

            let handle = unsafe {
                CreateFileMappingW(
                    INVALID_HANDLE_VALUE,
                    None,
                    PAGE_READWRITE,
                    0,
                    size,
                    windows::core::PCWSTR(name.as_ptr()),
                )
            }
            .map_err(|e| format!("CreateFileMappingW failed: {e}"))?;

            let ptr = unsafe { MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, 0) };
            if ptr.Value.is_null() {
                unsafe { let _ = CloseHandle(handle); }
                return Err(format!(
                    "MapViewOfFile failed: {}",
                    std::io::Error::last_os_error()
                ));
            }

            let ring = ptr.Value as *mut ShmRingBuffer;
            unsafe {
                (*ring).head.store(0, Ordering::Release);
                (*ring).tail.store(0, Ordering::Release);
                (*ring).drop_count.store(0, Ordering::Release);
            }

            Ok(ShmRing {
                ring,
                _handle: windows_handle::OwnedHandle(handle),
            })
        }

        #[cfg(not(any(unix, windows)))]
        Err("ShmRing::create: unsupported platform".to_string())
    }

    /// Write a serialized tree into the next available ring slot.
    /// Returns `true` if the slot was accepted, `false` on backpressure.
    pub fn write(&self, builder: &WireInitNodeBuilder) -> bool {
        let ring = unsafe { &*self.ring };
        let head = ring.head.load(Ordering::Acquire);
        let tail = ring.tail.load(Ordering::Acquire);
        let next_head = (head + 1) % MAX_SLOTS as u64;

        if next_head == tail {
            // Ring full — backpressure.
            ring.drop_count.fetch_add(1, Ordering::Relaxed);
            return false;
        }

        let slot_off = (head as usize) * WIRE_SLOT_SIZE;
        let slot = unsafe {
            std::slice::from_raw_parts_mut(
                (*self.ring).data.as_mut_ptr().add(slot_off),
                WIRE_SLOT_SIZE,
            )
        };

        let mut cursor = 0usize;
        if !builder.serialize(slot, &mut cursor) {
            return false;
        }

        ring.head.store(next_head, Ordering::Release);
        true
    }

    pub fn drop_count(&self) -> u32 {
        unsafe { (*self.ring).drop_count.load(Ordering::Relaxed) }
    }
}

// ── C-callable FFI surface (used by part3.c via extern declarations) ─────────

#[no_mangle]
pub extern "C" fn zcc_shm_ring_write(
    ring_ptr: *mut std::ffi::c_void,
    builder: *const WireInitNodeBuilder,
) -> i32 {
    if ring_ptr.is_null() || builder.is_null() {
        return 0;
    }
    let ring = unsafe { &*(ring_ptr as *const ShmRingBuffer) };
    let b = unsafe { &*builder };

    let head = ring.head.load(Ordering::Acquire);
    let tail = ring.tail.load(Ordering::Acquire);
    let next_head = (head + 1) % MAX_SLOTS as u64;
    if next_head == tail {
        ring.drop_count.fetch_add(1, Ordering::Relaxed);
        return 0;
    }

    let slot_off = (head as usize) * WIRE_SLOT_SIZE;
    // Safety: slot_off + WIRE_SLOT_SIZE <= SHM_RING_SIZE (enforced by MAX_SLOTS math).
    let slot = unsafe {
        std::slice::from_raw_parts_mut(
            (ring_ptr as *mut u8)
                .add(std::mem::offset_of!(ShmRingBuffer, data))
                .add(slot_off),
            WIRE_SLOT_SIZE,
        )
    };

    let mut cursor = 0usize;
    if b.serialize(slot, &mut cursor) {
        ring.head.store(next_head, Ordering::Release);
        1
    } else {
        0
    }
}
