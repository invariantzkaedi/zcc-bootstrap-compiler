; ZCC FORMAL VERIFICATION LAYER: push_lea_pop_triad AUTOMATED PROOF
(set-logic QF_ABV)

(declare-fun mem_0 () (Array (_ BitVec 64) (_ BitVec 64)))
(declare-fun rax_0 () (_ BitVec 64))
(declare-fun rbx_0 () (_ BitVec 64))
(declare-fun rcx_0 () (_ BitVec 64))
(declare-fun rdx_0 () (_ BitVec 64))
(declare-fun rsi_0 () (_ BitVec 64))
(declare-fun rdi_0 () (_ BitVec 64))
(declare-fun rsp_0 () (_ BitVec 64))
(declare-fun rbp_0 () (_ BitVec 64))
(declare-fun r8_0 () (_ BitVec 64))
(declare-fun r9_0 () (_ BitVec 64))
(declare-fun r10_0 () (_ BitVec 64))
(declare-fun r11_0 () (_ BitVec 64))
(declare-fun r12_0 () (_ BitVec 64))
(declare-fun r13_0 () (_ BitVec 64))
(declare-fun r14_0 () (_ BitVec 64))
(declare-fun r15_0 () (_ BitVec 64))

; --- PRE-OPTIMIZATION STATE ---
; push rax
(define-fun rsp_1 () (_ BitVec 64) (bvsub rsp_0 #x0000000000000008))
(define-fun mem_1 () (Array (_ BitVec 64) (_ BitVec 64)) (store mem_0 rsp_1 rax_0))
;     leaq -80(%rbp), %rax
(define-fun rax_1 () (_ BitVec 64) (bvsub rbp_0 #x0000000000000050))
; pop r11
(define-fun r11_1 () (_ BitVec 64) (select mem_1 rsp_1))
(define-fun rsp_2 () (_ BitVec 64) (bvadd rsp_1 #x0000000000000008))

; --- POST-OPTIMIZATION STATE ---
; mov rax, r11
(define-fun r11_post () (_ BitVec 64) rax_0)
;     leaq -80(%rbp), %rax
(define-fun rax_post () (_ BitVec 64) (bvsub rbp_0 #x0000000000000050))
(define-fun rsp_post () (_ BitVec 64) rsp_0)

; --- EQUIVALENCE PROOF Target ---
(assert (not (and (= rax_1 rax_post) (= r11_1 r11_post) (= rsp_2 rsp_post))))

(check-sat)
(get-model)
