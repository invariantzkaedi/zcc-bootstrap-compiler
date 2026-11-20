#!/usr/bin/env python3
"""
Standalone Upstream MicroPython v1.24.1 WASI Builder (`build_wasm/micropython.wasm`)
===================================================================================
Bundles strictly unmodified upstream MicroPython v1.24.1 core C sources
(`tests/embedded_micropython/{py,extmod,lib/re1.5}`) with `src/wasm_wasi_rt.c`
and `src/wasm_mpy_driver.c`, using ZCC's native WebAssembly Exception Handling
(`Tag` Section 13 + `try_table` + `throw`) to power `MICROPY_NLR_SETJMP`, and
compiles via `./zcc --target=wasm32` (and optionally probes `build_wasm/zcc.wasm`)
into standalone WASI Preview1 `build_wasm/micropython.wasm` with zero `env.*` imports.
"""

import argparse
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WASM = os.path.join(BUILD_WASM, "zcc.wasm")
MPY_DIR = os.path.join(REPO_ROOT, "tests", "embedded_micropython")
MPY_GENHDR_DIR = os.path.join(BUILD_WASM, "mpy_genhdr")
DEFAULT_BUNDLE = os.path.join(BUILD_WASM, "micropython_wasi_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_WASM, "micropython.wasm")

UPSTREAM_TARBALL_URL = "https://github.com/micropython/micropython/archive/refs/tags/v1.24.1.tar.gz"
UPSTREAM_TARBALL_SHA256 = "a291173de1d680786a66dd2381805b6d134defaecea670a560dc7e7d8de540ad"

# Exact pinned SHA-256 manifest of unmodified MicroPython v1.24.1 subset
UPSTREAM_FILE_SHA256 = {
    "LICENSE": "74111c31aa66f05e5dbd3f9f303653dfda1aa85b8525941afb115e62e3f3d18a",
    "extmod/modheapq.c": "768c4d1469133414ee3409a9f50664737f587d5dd83c88f04afe24cc9c87fda4",
    "extmod/modjson.c": "2a17e5601debe103f5fbcba81cb3a11810ca7043c81c7861b7512fbc8362405c",
    "extmod/modplatform.h": "cc0db6d2f7c9c327c1ca1e46e7ebd95f491c4cd31d3fe43f0477d673c2cbf79c",
    "extmod/modrandom.c": "11d851f6755e565494ec52559c84047b5ac39552e9b5e8c4cb8207bbc8eb8b1e",
    "extmod/modre.c": "4e00c3872c48d94ad61a1a5d364626dbbbc9cb525ca02fe758565bf4316dae90",
    "extmod/virtpin.h": "01ca10ba6550e770934e98cada00c0661a684deabab0743f877be9a51ccb6c06",
    "lib/re1.5/charclass.c": "3d14c3859c993d435c933de8459150017fa9ece2259bc5e44125e4a88c288d17",
    "lib/re1.5/compilecode.c": "a82b5bee701c9f58a15a5c35ff93ee4e73c6c6c434c5670b34323feffc59063c",
    "lib/re1.5/re1.5.h": "392bc751d21e09cb9287debcae2a63420d5f8985315262c4694abc183d720095",
    "lib/re1.5/recursiveloop.c": "332bc8cb4e3e95de6159266e63529dfc613631c2a1f9d89d252fbe4b8946599d",
    "py/argcheck.c": "86c5b2ccb5fd8ce404c4731e43faa3d8fddc587682eb13331045ec5982558616",
    "py/asmarm.h": "611b8a39a60c9c8eaea73ffc0a02c2dddadb4d47c93978ff72cf06642ef56121",
    "py/asmbase.h": "d8027e88d402223a51b3d7c9bd6e11bfd2be8e5e5a6cc01ad32d0af625cb44d5",
    "py/asmrv32.h": "22b8054157712bcb70bc289528b2f3e412c967054e0bd93b16c507b3aab89429",
    "py/asmthumb.h": "45ee482fa5fc57bc6eb4162bc342850e37b3ad5bb1367c85d06a10b3d6669b83",
    "py/asmx64.h": "a2fd56b7476fda5b417cec32dc310420f046bce75443770435102c3ed68d2053",
    "py/asmx86.h": "e19a71a2db4289c4b290882b220270582638c8eb49b6a2e8ffb482e8e4299b39",
    "py/asmxtensa.h": "fe9b55b47f26bccdce64cd1cd1f5dda801187e31c559440c83cd52d4adbc1b67",
    "py/bc.c": "3100ae35f3dce80d8171169405d98ac2bc5f1471b824ed5dfddebc997ade62a7",
    "py/bc.h": "bcadf7af4a74bb0e20b3933aa7ebf739c82d189eb9d04c65f4f8bec42e487fe2",
    "py/bc0.h": "2bef1611b0ff1f1dd5330d32b6bdfae7725b41b4e334743086f88032f3fad62f",
    "py/binary.c": "d3bf28d4ad6fa1667be566c4509c69f803579e25b3c2f6cc459649f29c143ec7",
    "py/binary.h": "20384c6076ad45b74af55ecb85358e5fb7d64c62f694ba1c13589b336ddf61c5",
    "py/builtin.h": "560ab7be62d2a055481b2e9cd74224073e50587d7a0f6efba25272bc0dfb7fed",
    "py/builtinevex.c": "05fb8fa9300db0eda95e3a94b32124d9a4ff0f343fadd6f6fcbdd0769373cca2",
    "py/builtinimport.c": "5af413e96350119ea7de481765a48a0b75d0c3676029ec2e3bd8575e4f7f9d10",
    "py/compile.c": "88fd2e50f154b7bca297276d0f4deb9d5aa748e36b6e1be3dc5b78522eb237b4",
    "py/compile.h": "295eea2353b8b2b5e8bb2e9af3bd51a4ff5ad5c1734297466405b9854965837f",
    "py/cstack.c": "28cc109277df56d500cd2f3a7848c85562fd77d555aae3cca5427f6e3ad90382",
    "py/cstack.h": "e861371b2308bd193dc7c6aa226ac982a4a4b0ac5a302f604bb3187811133b5b",
    "py/dynruntime.h": "e254a8e8aa0ff0bfa5e19fb611ed1a94695cb0bb7dfa1057c2c22f76c5df83f1",
    "py/emit.h": "89638f2ff35e359523dd9c6f1b51501b0547a02411a060abd31be6b8bc092871",
    "py/emitbc.c": "b525ce15662b18e8e7cff28c1809e45db31f853a409f529b03d18b8b85b05a61",
    "py/emitcommon.c": "db1e13afb69f7ef388b3e170e610bda25e86d85d1c13573e7968942b0aee7d7f",
    "py/emitglue.c": "59b14b634d7dabc1246f20e360b511ea8e652070d408137677f0f07b623e1b26",
    "py/emitglue.h": "04bc4314892d13491780cee9ad598a8c7637a66fe97428777910e41d499a2731",
    "py/formatfloat.c": "8a9e22023e19c048f748d3c368d4dff03d5a7cc0da1baa0ffb2ed1badb106703",
    "py/formatfloat.h": "96ffaf995d775c3e423dfd48868672c25751975b2ad016d4fd5a41325e0c07df",
    "py/frozenmod.h": "f0a3fca1f721811c1877d746d425cc2f42d79020476a58862ba3f63505601f8c",
    "py/gc.c": "1b5dd27fda71f256cbae3918f8c18cfaa5685db6eea2295967d955ae4139fb7c",
    "py/gc.h": "3634b7ef39e4b5c204c17948f18c5ec8865b6a7645399014f9c3ce547bb7e33f",
    "py/grammar.h": "2bca240287eb0489481fb153dfbd804084af45dfa366ea0a5a6d164927e218e3",
    "py/lexer.c": "24fc160f6dcb06b01450eaf32550a662e57e7706efd0920b7ae08f6764c6635c",
    "py/lexer.h": "0454dc6f631cef3815ab4d0de644a12ef07281c1950a58bae5e55760f817701e",
    "py/make_root_pointers.py": "297c295df46063992363ba8f28553d7d3537e8e44388720b8ec80b026333f163",
    "py/makecompresseddata.py": "175188a58fbd65300755f09dd94ba5aabd88afb5fca7c05d475a80956b9c5dec",
    "py/makemoduledefs.py": "a74b55dfd9dcb37c7fc0d6d1f37b1af77e16aa0ab385de6369a5d2ae699b5b9f",
    "py/makeqstrdata.py": "aa983b1fcafe0519eade3878b298b159fefa1f036ebd55f569a57c95018da308",
    "py/makeqstrdefs.py": "a8e14263a8c98e3e7e4373719ca4eec0907d920cb6dea13f40055b651d6745f3",
    "py/makeversionhdr.py": "216474a976bf336d3df8d70b9e30eb88419653eeb3742fc6b05e46a40aa6ef99",
    "py/malloc.c": "ddb159ff275dcc75fa4a9c118aabfb910294ad3f1d72b1042b3ebe9f75a65cdd",
    "py/map.c": "06c6fc5e49dd778b1abb4cd989b3705d21e6f338a8eef0474815b717ee7e33a0",
    "py/misc.h": "1f1036b01f9e1813a72ba964c87067e1281175955c7bab61cb730114e77eca77",
    "py/modarray.c": "edfe6431c34d6530810f646c0bc308b12f3c591f1e2a864276e11639184582e5",
    "py/modbuiltins.c": "408326aa3590794afede4778073bf5f46626bddfd7e73868398f980f26dd105a",
    "py/modcollections.c": "a289ee3bfcd0d5a681376c59fbac81c8d49c23476b2408110b6d642e648c8fd3",
    "py/moderrno.c": "217ebe3106df1611576bb1c1cb65f08d3f37785fd4b6a31c04b8e663ddf259ae",
    "py/modgc.c": "0288a1d2ebdf1208fb62311f1bf7cdde01710719b4584fcf4cbe14dcae7fec20",
    "py/modio.c": "e52eab94b66d9589ebf8496d834a84c285360ea5d80458e05fb46e42f8654428",
    "py/modmath.c": "ed11140985f494aa0212d9efcb9788b917820426921062f0a4c6236d71ed6da8",
    "py/modmicropython.c": "2e3042ac9220f5fa51a3135bb2e1e39c0a8488fd60257e43a751e07fd90711b2",
    "py/modstruct.c": "8a9e56a4cc352486044e55e0d328f923c6ab57e11be9b40266b81cb85c9714c0",
    "py/modsys.c": "12b5e7df3997573f0ce09a9f3c5f9e024a1bf9055f5bbf1795e463a5f5ba2d2b",
    "py/mpconfig.h": "84b3af3b445a13e33468e02dc2b00969fd12347b7c8bcee67eea3e98b79bb49d",
    "py/mperrno.h": "2a274231f63f918e142605572a7d0c4eac473c34ed0c3560ea07ec8850c225f0",
    "py/mphal.h": "a653b36a221157657396ddfb46afdd137b01ed9a1fb3b8bfdd5ede2ca28c8b89",
    "py/mpprint.c": "27410e0e5adb39c1c36b02425b227c8ead8d7a53c285438ba35d1fdafd42a2be",
    "py/mpprint.h": "738d44217360eee7cf583cfae45852f89aa3e061190218fd1af108a5fd0ba47d",
    "py/mpstate.c": "7418dec6f7c87e939383f01f0dd7e5fae6bd4eebc75fd7b52039c6915af39bce",
    "py/mpstate.h": "cbe698434600b495c7299ea3e6e2a19499f288e6ae9aff368fce331c91457dd4",
    "py/mpthread.h": "f53d0306470237f66a36054a56c9471beed8b958e311ba36a8dcfa8f41107222",
    "py/mpz.c": "946716d407d86686691cdb50b991748df1e817d1c3afea13cbc896d6f3b39bef",
    "py/mpz.h": "aac215e4f5fa4da138bee75856d2443198147fe502dc69fe954b80bfab010136",
    "py/nativeglue.h": "d31d75224f193d880b8b5e0f42178b4e43f3c2a5e340314d4d7fbc393ec27b45",
    "py/nlr.c": "d555fc9ae5e7b443f3eb549a2961cb8599a26b451829d743f109a6a9e8069e6d",
    "py/nlr.h": "19f49642eb41ce0fec8584d87ea00428bae2599188844148624975e1fa79eba8",
    "py/nlrsetjmp.c": "bb8dadb5865f43fc5d005a8daa65b9c8065777bdc4e912a6a5eecf98b442659e",
    "py/obj.c": "1da39829cebde53589487d9d4c78b811fb752b7163699d3409f6de8615e550dd",
    "py/obj.h": "dcc848214e39133877a35c82f9b031c43ef506586d90ba32843b89301d6a6d1b",
    "py/objarray.c": "859f5260eb40ca8bb19a628ab330fa5c801e174c2a5e1d363248d2dd83afb183",
    "py/objarray.h": "a175c31282e5a4826178b347ef9d61b2f58cb91857f6f680bfa39eb0702a89ea",
    "py/objattrtuple.c": "760b00d8f946ebd2060284b3a3b40f42d155477c930d0fa27bdd823269ff0e3d",
    "py/objbool.c": "a72061d84f78c25ecf7ba940301034233daaf0d0bb01bf1c51decf6f53f6c29b",
    "py/objboundmeth.c": "b574405400e1b8988566533bc109c754c9449eeeee7e7a4f5e3fb17294c3460e",
    "py/objcell.c": "6d8e754fd90ca250a00ed045aa54b183bb30be7e4e17382de0139c74aad31541",
    "py/objclosure.c": "d2591f22a569173dbc3430a62063533fdee054bdb8d2445977da049beff7ae97",
    "py/objcomplex.c": "c48682340306172ac0d4777d3c39d119b41ce84c9dcbd1b8c330ae1fcee512f7",
    "py/objdeque.c": "e3f64733528ab3361b7c9825e200d961e0c8d2874eeb7a0a6c2c6e9cbd3852b5",
    "py/objdict.c": "f3352ca1f28305f6ce5fc2fafcd33f1992138ca183d8c1376370ae53f3e0629e",
    "py/objenumerate.c": "39a4b3fbfa42714550b69029872c5ca76d6953a00b84e497c2f4142b80fef513",
    "py/objexcept.c": "cc0d9c636ea4f497e40d90d612af019b8ea70703f8fa817f9fc6856d6cba0c95",
    "py/objexcept.h": "6886352bdd27ad71ea4330d50e38ba48b45f0dff4ee19a4f5edfcffa9dafb830",
    "py/objfilter.c": "956e57c0d2a2fe1857b66e9a222fb6d1f97d6de3796feb575c84eb2645bff1d5",
    "py/objfloat.c": "e6356437b36b9f15b8190b081fe15d600c633de548d6dd18983af5e6abfcfc94",
    "py/objfun.c": "b385480ae3d4c17c366965e9b2b49e25cf5c0bafdaf121c1a1f903f0e2113081",
    "py/objfun.h": "2851fe4c0d564bef90790edd334334c32e9188805262317a91ded6fa7ea702cc",
    "py/objgenerator.c": "3a776671a6742d2e8b835161bb57e90141a41e0a021ee78904b6639741246f4e",
    "py/objgenerator.h": "e91fbeaec36d841a8ea030f21b57c4b71d1bf6b872c92f1bcc48cca7314a62aa",
    "py/objgetitemiter.c": "af9bb3530c2f28b0bf8888e93bd97d2bb1a1f46dd6cf1e743e61c2db1e7d3306",
    "py/objint.c": "7899163e0a5d452e5c5e66a5dd273e708e857372acf7f12c8ce336d148934366",
    "py/objint.h": "0ea72aaed5ce4a42a8b4e3193c70edfafa7914ee211656c907f12607b84b87a2",
    "py/objint_longlong.c": "754a36bbb0bb6ee5591f1ca3db974d5a2958b74e1217a1177fe108dd9d9faf20",
    "py/objint_mpz.c": "707b3146bb016444b0e1da6495d28aa032c97d57a06d81af2a8e22f5b1d3352c",
    "py/objlist.c": "f3b4e88c7998aa22f64e673de1bd851ec0cb0069215c6855ede3f9e54706c6bb",
    "py/objlist.h": "efd7c3eae1e3bb9cb1af44b29cac8934fe319e851c60607fe6547e2612c50fcb",
    "py/objmap.c": "bc27aa266e89a7e5d19cfc66b117b34c960cacb630685aea13c21e73400631a9",
    "py/objmodule.c": "8c11cf7b3373a08090e63200352b5dd9fabace6c12a41d73152149c4294527e0",
    "py/objmodule.h": "2f0e7ca9e0ed845276e4b2cfea1de5213ffde39f8e51dacdf4a7fc87ac86ada1",
    "py/objnamedtuple.c": "30ab44ec723f4eae08a50a737cbf03e3a764d6c7e00cb2baa269338baa92082e",
    "py/objnamedtuple.h": "317ebb4cc77661b643e402dbedce79cc6838163333479ab20475010f7379c5b7",
    "py/objnone.c": "6df9db9ef493db55ad199954bf9afd5303e5cf2dd0a570d15d716f0e838137f4",
    "py/objobject.c": "6aef367bb57dfe1b72997b3a397c713fb23af911cd573d593f150bb2849bdec3",
    "py/objpolyiter.c": "d23b2b2f4aabca5b7691ea447d34a8b68c0e7fdb4a0e4cabebd1e51654a903ac",
    "py/objproperty.c": "213bf6e5b95df6e53f62085087bb4afca7229c7c23de061615581380a3a418c1",
    "py/objrange.c": "077a9d4c581003034d3dd8a5bf02d5fbb2c70cda5cae48c7ef924f22f571fab3",
    "py/objreversed.c": "b4b1acccabd939e5a863b7a7340293f4ac0b32a720ebf13ff281412be962675d",
    "py/objringio.c": "1ae8bb2c178bf70f7c9216323dd742c14afbe9a3f7abb6d480aa4855de1005e0",
    "py/objset.c": "88515d14207fa60f075b5414a98b1060f7ea0a2dca94fa0fa9d24149313af755",
    "py/objsingleton.c": "3fe621c937e1e03616fdeab768ec40ceb7ab72d94b5c26373a54885e9a76536f",
    "py/objslice.c": "4adc08410982d8e3ff4f200260e31c7a1cda94b0909a5d75253f0976b21aea98",
    "py/objstr.c": "9abd7e2b16c8076d72314179072513409467fb13b0dae5201cc1970c9d3525f3",
    "py/objstr.h": "7bd9671f19bfd2370a0bbb25340c650576f5c8a0ebd300ce42105568dd0a0ede",
    "py/objstringio.c": "33308e411ac58725f53a16ba69568e58875aed17d9b48eab70f553d7d3b94d8c",
    "py/objstringio.h": "6fb67485e744d774b311ed5dc733f506cc53ad53fd638533a97def28ed12b7fe",
    "py/objstrunicode.c": "f5c74c2d67b696c29976fe1ae6e7801243960ed7b8a43da1e65763b769f0dcaf",
    "py/objtuple.c": "194d51e76723efeee0e7d1ac87ed8bb309b08a757bd36b830554c3f36ae685ec",
    "py/objtuple.h": "f8c5ecb0dd551f01312d1e96b55b0336d30fb57614f981854a554b5b9978ff79",
    "py/objtype.c": "51477c30ead601e6142079d761bda3f928a239d719645d0a73d5843e2d1fd3a6",
    "py/objtype.h": "33d5f7f72c9f787168ed26a188b46137312d6371781b5e625e93f6d297840060",
    "py/objzip.c": "e1da8aeef9c75080e6c148ae546d2112068622e726dce57d3b0028cc700df15f",
    "py/opmethods.c": "3981aacede8bf930add61f4d99e35903578b14534780eecd4adf9f4d8d70f2ac",
    "py/pairheap.c": "2a054499f0e0f41065a17345e65fa20f2660e3c5760cd90583d56fe4b7145a53",
    "py/pairheap.h": "98c0fae4fa56e95d01b2df01eac1502afc8b6c5fda4d848a914d853e73a17397",
    "py/parse.c": "bcc361204ff3411420b0f3554c0dedfff1ba037fb4c668d4c4eab5db2b4342d8",
    "py/parse.h": "fad62a39725af452332b000a71226268124ef568750b838dbb237b770d1c73ca",
    "py/parsenum.c": "9221e2561c7efa4d0fb4ac0634fcdcbf15d9d964a6a9f9843223f9518f6779a4",
    "py/parsenum.h": "67b51b34930e2cdf442b18a78e817b4861ba6f370de7b42419bc828a7347f978",
    "py/parsenumbase.c": "2a91f47a0b1600db3944ff2806e61f4bed9cf0e716d9a0f6a21e4d1a0e99df8d",
    "py/parsenumbase.h": "8421644f6e304103a1efff9d892ae64672fda4e346d1e5ac05e9b75ce562fb61",
    "py/persistentcode.h": "cf1aff132bf4fb655d8c3f0273bfc2b8b22efe16049ade6982e3122384640aa4",
    "py/profile.h": "0fc81211e1491f7253caccd9184a04b29a43fee0f9273fe48a9478cc5da5b29a",
    "py/pystack.c": "dee5cc2cd964a3b1c4037d850a285dafa52a7d12792102fd8607b838cbe6db71",
    "py/pystack.h": "04eb5e96811815a2f88152d84fa306e8ef1317956880d45e4cc99b8d1e3b3485",
    "py/qstr.c": "ddae4f2cbf9ef732f0982426c392d975ed735b495b99429e14d92ebf83ea068f",
    "py/qstr.h": "e284737a0bcba689d03ab850af80607fd096734c63b5d5b05b3ccff42ed64c61",
    "py/qstrdefs.h": "235533e514f12420e7b81e68abab1c7783e21ef1c8edc3d42fb587f4775a746d",
    "py/reader.c": "04576781ca4a7daa68e796167a8cd69e7cd86c8208685554f6dd99108f1045ce",
    "py/reader.h": "c1dc88b2d932b111289201b85ebcf952c36a5a543204fd316e983817954d0cc1",
    "py/repl.c": "005b3a8fa30c6ccee082574568a85f268921c2af00c442447f70763f6996b385",
    "py/repl.h": "a9a6b1877013797b5daf436247ef6232db5f4f6f03abc1817e611f2b8c6fbb84",
    "py/ringbuf.c": "4a4922402e5e0421e739e9e543221d571ca6f624d378a2f7a2dc30fd7e3b5939",
    "py/ringbuf.h": "8c228307b2ff0e2bb15b4b8753dcc049e85de793cf8ea908b2051ad33e4136d3",
    "py/runtime.c": "365d8534c072a8f15f81a76f5a149e5c2ecd624a675ca4a1a6b611806dd7d497",
    "py/runtime.h": "b748d07d801b561b28047f374b55a5b508a4766ae8e03adcf45472a9df5a5bd2",
    "py/runtime0.h": "af5324ccb874e6e5c2bcc2c68f53a1b21a5a6b8d8b74619cc94ea1beebb7d8cb",
    "py/runtime_utils.c": "7d3a5c156d12d232e1679dbd3353ab9d54fbb84bd2ad9f4ba69d80564a8a84c2",
    "py/scheduler.c": "47e4d007ae384faef356e423b19081ff23638b21a65a3a4b4c410c0a9d46b1bc",
    "py/scope.c": "2b13b4cdb0b9783e5391a4b02fa1ed0849144bebc53bce2eecbdf79b729c1694",
    "py/scope.h": "a35a9bd623cb8269d9f9eeb90205de3f466029689f8f7bc57cbece3bd9bd7918",
    "py/sequence.c": "e909ff30ae8f773223712178b991fc8ba5620011effcf771e03db460c4edd348",
    "py/smallint.c": "7cfb20c826c5fcb75a267de3e473c22612e593436449375cc47e61761bd5bf3e",
    "py/smallint.h": "ca5c75b38ee778b4b17250cf319afe9cceb28f4329539c90d64d767b5ae8765a",
    "py/stackctrl.c": "eb2e6de86a3584d3665f6e79d2f948eda76c86efb6d93981fa135d34ef3a7cbb",
    "py/stackctrl.h": "4bed997c067bcafecf3e4d813297dad1f02e89efc219f4b3384f47fe522ed5f5",
    "py/stream.c": "c1e822a9a3e2315458ac9ddfb49428e8137318a91ca5729b3ac22c9d66f0c170",
    "py/stream.h": "1de863394f0918f0f542c2cba6286e55cbf1d59353c4fc025038b25b4b0b096c",
    "py/unicode.c": "bd2dbe17d10aef5200bf81241fcf05603bba6a60efb711d67a78491be9c6887f",
    "py/unicode.h": "ee811e44795dbe49fee416ea974f843334b63946482c97bd94845ea1cee1b729",
    "py/vm.c": "28e6344248b74f5bc9d74342781d760f3b0bd36141f878335a89dad746d69932",
    "py/vmentrytable.h": "46e773fe111cb397ce396d564c931701c2e0fa2d89acd85d3401461dec676b31",
    "py/vstr.c": "78d13b96e4262aa0a10fabbceea949ec2ed3506daf1b9677e5032b0e4d21c4f7",
    "py/warning.c": "aa944a9d5c3b245a3cee78a1e0aa7fa15e2f7b2a177d89c99b3dea687d87fc75",
    "shared/readline/readline.h": "0d64d7f504c1a3520b68800bc45cac66ce6b932b8cacdab8a0ee0b1d14ebe723",
}

PY_EXCLUDE = {
    "emitnative.c",
    "emitinlinethumb.c",
    "emitinlinextensa.c",
    "emitinlineriscv.c",
    "emitnarm.c",
    "emitndebug.c",
    "emitnrv32.c",
    "emitnthumb.c",
    "emitnx64.c",
    "emitnx86.c",
    "emitnxtensa.c",
    "emitnxtensawin.c",
    "asmbase.c",
    "asmx64.c",
    "asmx86.c",
    "asmthumb.c",
    "asmarm.c",
    "asmxtensa.c",
    "asmriscv.c",
    "asmrv32.c",
    "nlrx86.c",
    "nlrx64.c",
    "nlrthumb.c",
    "nlraarch64.c",
    "nlrxtensa.c",
    "nlrriscv.c",
    "nlrpowerpc.c",
    "nlrmips.c",
    "nlrrv32.c",
    "nlrrv64.c",
    "persistentcode.c",
    "modthread.c",
    "profile.c",
    "showbc.c",
    "frozenmod.c",
    "nativeglue.c",
    "modcmath.c",
    "builtinhelp.c",
}

STRIP_SYS_HEADERS = {
    "stdbool.h",
    "stdint.h",
    "stddef.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "stdarg.h",
    "limits.h",
    "float.h",
    "math.h",
    "ctype.h",
    "errno.h",
    "setjmp.h",
    "assert.h",
    "inttypes.h",
    "unistd.h",
    "alloca.h",
    "time.h",
    "sys/types.h",
    "sys/time.h",
    "sys/stat.h",
    "fcntl.h",
}

MPCONFIGPORT_H = r"""#ifndef ZCC_MPCONFIGPORT_H
#define ZCC_MPCONFIGPORT_H

#undef __x86_64__
#undef __LP64__
#define MPZ_DIG_SIZE (16)

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)

#define MICROPY_ENABLE_COMPILER             (1)
#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_ENABLE_PYSTACK              (1)
#define MICROPY_PY_GC                       (1)
#define MICROPY_HELPER_REPL                 (0)
#define MICROPY_HELPER_LEXER_UNIX           (0)
#define MICROPY_ENABLE_SOURCE_LINE          (1)
#define MICROPY_ENABLE_DOC_STRING           (0)
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_DETAILED)
#define MICROPY_WARNINGS                    (1)
#define MICROPY_OPT_COMPUTED_GOTO           (0)
#define MICROPY_ROM_TEXT_COMPRESSION        (0)
#define MICROPY_NLR_SETJMP                  (1)
#define MICROPY_GCREGS_SETJMP               (0)
#define MICROPY_LONGINT_IMPL                (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_PY_BUILTINS_COMPLEX         (1)
#define MICROPY_PY_MATH                     (1)
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS   (0)
#define MICROPY_PY_CMATH                    (0)
#define MICROPY_PY_IO                       (1)
#define MICROPY_PY_IO_IOBASE                (1)
#define MICROPY_PY_JSON                     (1)
#define MICROPY_PY_RE                       (1)
#define MICROPY_PY_RANDOM                   (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS       (1)
#define MICROPY_PY_SYS_EXIT                 (1)
#define MICROPY_PY_SYS_PATH                 (1)
#define MICROPY_PY_SYS_ARGV                 (1)
#define MICROPY_PY_SYS_STDFILES             (0)
#define MICROPY_PY_SYS_MAXSIZE              (1)
#define MICROPY_PY_SYS_EXC_INFO             (1)
#define MICROPY_PY_TIME                     (0)
#define MICROPY_PY_SELECT                   (0)
#define MICROPY_PY_OS                       (0)
#define MICROPY_PY_PLATFORM                 (0)
#define MICROPY_PY_BINASCII                 (0)
#define MICROPY_PY_HASHLIB                  (0)
#define MICROPY_PY_HEAPQ                    (1)
#define MICROPY_PY_COLLECTIONS              (1)
#define MICROPY_PY_COLLECTIONS_DEQUE        (1)
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT  (1)
#define MICROPY_PY_STRUCT                   (1)
#define MICROPY_PY_ERRNO                    (1)
#define MICROPY_PY_ASYNCIO                  (0)
#define MICROPY_PY_DEFLATE                  (0)
#define MICROPY_PY_FRAMEBUF                 (0)
#define MICROPY_PY_VFS                      (0)
#define MICROPY_PY_CRYPTOLIB                (0)
#define MICROPY_PY_SSL                      (0)
#define MICROPY_PY_WEBSOCKET                (0)
#define MICROPY_PY_UCTYPES                  (0)
#define MICROPY_READER_POSIX                (0)
#define MICROPY_READER_VFS                  (0)
#define MICROPY_PERSISTENT_CODE_LOAD        (0)
#define MICROPY_PERSISTENT_CODE_SAVE        (0)
#define MICROPY_KBD_EXCEPTION               (0)
#define MICROPY_CPYTHON_COMPAT              (1)
#define MICROPY_PY_DELATTR_SETATTR          (1)
#define MICROPY_PY_FSTRINGS                 (1)
#define MICROPY_PY_BUILTINS_HELP            (0)
#define MICROPY_PY_MICROPYTHON_MEM_INFO     (1)
#define MICROPY_STACK_CHECK                 (0)
#define MICROPY_USE_INTERNAL_PRINTF         (0)
#define MICROPY_USE_INTERNAL_ERRNO          (1)

#define MICROPY_HW_BOARD_NAME "ZCC-WASI-WASM32"
#define MICROPY_HW_MCU_NAME   "wasm32"
#define MICROPY_PY_SYS_PLATFORM "wasi"

typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

#define MP_STATE_PORT MP_STATE_VM
#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p)))
#define MICROPY_MIN_USE_C_STDLIB (1)
#define MP_PLAT_PRINT_STRN(str, len) mp_hal_stdout_tx_strn(str, len)

#define __builtin_expect(x, v) (x)
#define __builtin_unreachable() do {} while (0)
#define __builtin_clz(x) zcc_mpy_clz(x)
#define __builtin_ctz(x) zcc_mpy_ctz(x)

static inline int zcc_mpy_clz(unsigned int x) {
    if (x == 0U) return 32;
    int n = 0;
    while ((x & 0x80000000U) == 0U) { x <<= 1; n++; }
    return n;
}

static inline int zcc_mpy_ctz(unsigned int x) {
    if (x == 0U) return 32;
    int n = 0;
    while ((x & 1U) == 0U) { x >>= 1; n++; }
    return n;
}

#define MICROPY_PORT_ROOT_POINTERS
#endif
"""

MPHALPORT_H = """#ifndef ZCC_MPHALPORT_H
#define ZCC_MPHALPORT_H
#include <stddef.h>
static inline void mp_hal_set_interrupt_char(int c) { (void)c; }
void mp_hal_stdout_tx_strn(const char *str, size_t len);
#endif
"""

SPECIAL_PRE_QSTRS = (
    "<stdin>",
    "<string>",
    "<module>",
    "<lambda>",
    "<dictcomp>",
    "<listcomp>",
    "<setcomp>",
    "<genexpr>",
)

SPECIAL_QSTR_ENCODED_NAMES = {
    "_lt_stdin_gt_",
    "_lt_string_gt_",
    "_lt_module_gt_",
    "_lt_lambda_gt_",
    "_lt_dictcomp_gt_",
    "_lt_listcomp_gt_",
    "_lt_setcomp_gt_",
    "_lt_genexpr_gt_",
    "_0x0a_",
    "_space_",
    "_star_",
    "_slash_",
    "_percent_",
    "_dot_",
    "_hyphen_",
    "_plus_",
    "_colon_",
    "_comma_",
    "_equals_",
    "_hash_",
    "_at_sign_",
    "_lt_dict_gt_",
    "_brace_open__colon__hash_b_brace_close_",
    "_brace_open__colon__hash_x_brace_close_",
    "_brace_open__colon__hash_o_brace_close_",
}


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found at /home/ZKAEDI/.local/bin/node or on PATH")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def inspect_wasm_v8(node_bin: str, wasm_path: str) -> dict:
    with open(wasm_path, "rb") as f:
        data = f.read()
    pos = 8
    section_ids = []
    while pos < len(data):
        sec_id = data[pos]
        pos += 1
        sz = 0
        shift = 0
        while pos < len(data):
            b = data[pos]
            pos += 1
            sz |= (b & 0x7F) << shift
            if (b & 0x80) == 0:
                break
            shift += 7
        section_ids.append(sec_id)
        pos += sz

    js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
let compiled = false;
let imports = [];
let exportsList = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    compiled = true;
    imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
    exportsList = WebAssembly.Module.exports(mod).map(e => `${e.name}:${e.kind}`);
  } catch (e) {
    err = String(e);
  }
}
console.log(JSON.stringify({valid, compiled, bytes: b.length, imports, exportsList, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    info = json.loads(res.stdout.strip().splitlines()[-1])
    info["sectionIds"] = section_ids
    return info


def run_node_wasi(node_bin: str, wasm_path: str, args, timeout: int = 300) -> subprocess.CompletedProcess:
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({
  version: 'preview1',
  args: cfg.args,
  env: {},
  preopens: { '.': '.' },
  returnOnExit: true,
});
const imp = w.getImportObject();
const wasi = imp.wasi_snapshot_preview1;
for (const k of Object.keys(wasi)) {
  const orig = wasi[k];
  wasi[k] = (...a) => orig.apply(wasi, a);
}
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const mod = await WebAssembly.compile(bytes);
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = w.start(inst);
  process.exit(rc | 0);
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(wasm_path), "args": args}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "--no-warnings", "-e", js, cfg_path],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def ensure_micropython_sources(mpy_dir: str = MPY_DIR) -> None:
    """Verify pinned MicroPython v1.24.1 sources on disk, fetching and verifying tarball if absent."""
    os.makedirs(mpy_dir, exist_ok=True)
    all_present = all(os.path.isfile(os.path.join(mpy_dir, rel)) for rel in UPSTREAM_FILE_SHA256)
    if not all_present:
        print(f"[FETCH] Downloading upstream MicroPython v1.24.1 from {UPSTREAM_TARBALL_URL}...", flush=True)
        with tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as tf:
            tmp_tgz = tf.name
        try:
            with urllib.request.urlopen(UPSTREAM_TARBALL_URL, timeout=120) as resp:
                with open(tmp_tgz, "wb") as out_f:
                    shutil.copyfileobj(resp, out_f)
            got_tar_sha = sha256_file(tmp_tgz)
            if got_tar_sha != UPSTREAM_TARBALL_SHA256:
                raise RuntimeError(
                    f"MicroPython tarball SHA-256 mismatch: expected {UPSTREAM_TARBALL_SHA256}, got {got_tar_sha}"
                )
            prefix = "micropython-1.24.1/"
            with tarfile.open(tmp_tgz, mode="r:gz") as tf_gz:
                for rel in UPSTREAM_FILE_SHA256:
                    member = tf_gz.getmember(prefix + rel)
                    src_stream = tf_gz.extractfile(member)
                    if src_stream is None:
                        raise RuntimeError(f"Tarball missing member {prefix + rel}")
                    dst = os.path.join(mpy_dir, rel)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    with open(dst, "wb") as dst_f:
                        shutil.copyfileobj(src_stream, dst_f)
        finally:
            if os.path.exists(tmp_tgz):
                os.remove(tmp_tgz)

    for rel, expected_sha in UPSTREAM_FILE_SHA256.items():
        full = os.path.join(mpy_dir, rel)
        if not os.path.isfile(full):
            raise RuntimeError(f"Missing MicroPython source file after ensure_micropython_sources: {full}")
        got_sha = sha256_file(full)
        if got_sha != expected_sha:
            raise RuntimeError(f"SHA-256 mismatch for {full}: expected {expected_sha}, got {got_sha}")


def strip_c_line_comments_and_rename_class(code: str) -> str:
    """Strip // line comments (for multi-line macro pasting) and rename C identifier `class` outside strings/comments."""
    out = []
    i = 0
    n = len(code)
    while i < n:
        c = code[i]
        if c == '"' or c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                ch = code[i]
                out.append(ch)
                i += 1
                if ch == "\\" and i < n:
                    out.append(code[i])
                    i += 1
                elif ch == q:
                    break
        elif c == "/" and i + 1 < n and code[i + 1] == "*":
            out.append("/*")
            i += 2
            while i + 1 < n and not (code[i] == "*" and code[i + 1] == "/"):
                out.append(code[i])
                i += 1
            if i + 1 < n:
                out.append("*/")
                i += 2
        elif c == "/" and i + 1 < n and code[i + 1] == "/":
            while i < n and code[i] != "\n":
                i += 1
        elif c.isalpha() or c == "_":
            j = i + 1
            while j < n and (code[j].isalnum() or code[j] == "_"):
                j += 1
            tok = code[i:j]
            out.append("mpy_cls_" if tok == "class" else tok)
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def collect_mpy_c_sources(mpy_dir: str) -> list:
    py_files = sorted(
        f for f in glob.glob(os.path.join(mpy_dir, "py", "*.c")) if os.path.basename(f) not in PY_EXCLUDE
    )
    extmod_files = [
        os.path.join(mpy_dir, "extmod", "modheapq.c"),
        os.path.join(mpy_dir, "extmod", "modjson.c"),
        os.path.join(mpy_dir, "extmod", "modre.c"),
        os.path.join(mpy_dir, "extmod", "modrandom.c"),
    ]
    return py_files + extmod_files


def collect_guarded_headers(mpy_dir: str) -> set:
    guarded = set()
    for sub in ("py", "extmod", "lib/re1.5", "shared/readline"):
        for hp in glob.glob(os.path.join(mpy_dir, sub, "*.h")):
            rel = os.path.relpath(hp, mpy_dir).replace("\\", "/")
            with open(hp, "r", encoding="utf-8", errors="replace") as f:
                head = f.read(1600)
            if "#ifndef MICROPY_INCLUDED_" in head or "#ifndef RE1_5_H" in head:
                guarded.add(rel)
    return guarded


def write_bundle_and_genhdr(bundle_path: str = DEFAULT_BUNDLE, mpy_dir: str = MPY_DIR, gen_root: str = MPY_GENHDR_DIR) -> tuple:
    ensure_micropython_sources(mpy_dir)
    genhdr_dir = os.path.join(gen_root, "genhdr")
    os.makedirs(genhdr_dir, exist_ok=True)
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)

    with open(os.path.join(gen_root, "mpconfigport.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(MPCONFIGPORT_H)
    with open(os.path.join(gen_root, "mphalport.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(MPHALPORT_H)
    with open(os.path.join(genhdr_dir, "mpversion.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(
            '#define MICROPY_GIT_TAG "v1.24.1"\n'
            '#define MICROPY_GIT_HASH "v1.24.1-zcc"\n'
            '#define MICROPY_BUILD_DATE "2026-11-19"\n'
        )

    all_c = collect_mpy_c_sources(mpy_dir)
    guarded_hdrs = collect_guarded_headers(mpy_dir)

    # Scan enabled modules & root pointers across selected C modules
    mod_regs = []
    rp_regs = []
    for fn in all_c:
        with open(fn, "r", encoding="utf-8", errors="replace") as f:
            txt = f.read()
        for m in re.finditer(r"MP_REGISTER_(?:EXTENSIBLE_)?MODULE\s*\([^;]+\)\s*;", txt):
            line = m.group(0)
            if "MP_QSTR_cmath" in line or "MP_QSTR__thread" in line:
                continue
            mod_regs.append(line)
        for m in re.finditer(r"MP_REGISTER_ROOT_POINTER\s*\([^;]+\)\s*;", txt):
            rp_regs.append(m.group(0))

    mod_i = os.path.join(genhdr_dir, "module_regs.i")
    with open(mod_i, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(mod_regs) + "\n")
    r_m = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "makemoduledefs.py"), mod_i],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(os.path.join(genhdr_dir, "moduledefs.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(r_m.stdout)

    rp_i = os.path.join(genhdr_dir, "root_pointers.i")
    with open(rp_i, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rp_regs) + "\n")
    r_rp = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "make_root_pointers.py"), rp_i],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(os.path.join(genhdr_dir, "root_pointers.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(r_rp.stdout)

    # Initial empty qstrdefs.generated.h so ZCC --pp-only -DNO_QSTR can scan every MP_QSTR_* reference
    qgen_path = os.path.join(genhdr_dir, "qstrdefs.generated.h")
    with open(qgen_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("")

    seen_hdrs = set()

    def process_src(raw: str, cur_dir: str) -> str:
        raw = raw.replace("\r\n", "\n").replace("\r", "\n")
        raw = strip_c_line_comments_and_rename_class(raw)
        out = []
        for line in raw.splitlines(keepends=True):
            m_ang = re.match(r"^\s*#\s*include\s*<([^>]+)>", line)
            if m_ang and m_ang.group(1) in STRIP_SYS_HEADERS:
                out.append(f"/* stripped <{m_ang.group(1)}> */\n")
                continue
            m_q = re.match(r'^\s*#\s*include\s*"([^"]+)"', line)
            if m_q:
                inc = m_q.group(1)
                cand = inc
                if not os.path.isfile(os.path.join(mpy_dir, cand)):
                    rel_c = os.path.relpath(os.path.join(cur_dir, inc), mpy_dir).replace("\\", "/")
                    if os.path.isfile(os.path.join(mpy_dir, rel_c)):
                        cand = rel_c
                if cand in guarded_hdrs:
                    if cand in seen_hdrs:
                        out.append(f'/* dedup "{cand}" */\n')
                        continue
                    seen_hdrs.add(cand)
                    with open(os.path.join(mpy_dir, cand), "r", encoding="utf-8", errors="replace") as hf:
                        out.append(f"/* >>> BEGIN HDR {cand} */\n")
                        out.append(process_src(hf.read(), os.path.dirname(os.path.join(mpy_dir, cand))))
                        out.append(f"\n/* <<< END HDR {cand} */\n")
                    continue
                if cand.startswith("lib/re1.5/") and os.path.isfile(os.path.join(mpy_dir, cand)):
                    with open(os.path.join(mpy_dir, cand), "r", encoding="utf-8", errors="replace") as cf:
                        out.append(f"/* >>> BEGIN INC {cand} */\n")
                        out.append(process_src(cf.read(), os.path.dirname(os.path.join(mpy_dir, cand))))
                        out.append(f"\n/* <<< END INC {cand} */\n")
                    continue
            out.append(line)
        return "".join(out)

    parts = [
        "/* Auto-generated Upstream MicroPython v1.24.1 WASI Bundle (zero-binutils, zero env.* imports) */\n",
        '#include "src/wasm_wasi_rt.c"\n',
        '#include "mpconfigport.h"\n',
    ]
    for ch in ("py/mpconfig.h", "py/misc.h"):
        seen_hdrs.add(ch)
        with open(os.path.join(mpy_dir, ch), "r", encoding="utf-8", errors="replace") as hf:
            parts.append(f"/* >>> BEGIN CORE HDR {ch} */\n")
            parts.append(process_src(hf.read(), os.path.join(mpy_dir, "py")))
            parts.append(f"\n/* <<< END CORE HDR {ch} */\n")
    parts.append("#undef MP_STATIC_ASSERT_NONCONSTEXPR\n#define MP_STATIC_ASSERT_NONCONSTEXPR(cond) ((void)1)\n")
    for ch in ("py/qstr.h", "py/obj.h", "py/runtime.h"):
        if ch not in seen_hdrs:
            seen_hdrs.add(ch)
            with open(os.path.join(mpy_dir, ch), "r", encoding="utf-8", errors="replace") as hf:
                parts.append(f"/* >>> BEGIN CORE HDR {ch} */\n")
                parts.append(process_src(hf.read(), os.path.join(mpy_dir, "py")))
                parts.append(f"\n/* <<< END CORE HDR {ch} */\n")

    for fn in all_c:
        rel = os.path.relpath(fn, mpy_dir).replace("\\", "/")
        parts.append(f"\n/* ===== BEGIN tests/embedded_micropython/{rel} ===== */\n")
        parts.append(f'#line 1 "tests/embedded_micropython/{rel}"\n')
        with open(fn, "r", encoding="utf-8", errors="replace") as f:
            parts.append(process_src(f.read(), os.path.dirname(fn)))
        parts.append(f"\n/* ===== END tests/embedded_micropython/{rel} ===== */\n")
        if os.path.basename(fn) == "malloc.c":
            parts.append("#undef malloc\n#undef free\n#undef realloc\n#undef calloc\n")

    drv = os.path.join(REPO_ROOT, "src", "wasm_mpy_driver.c")
    if not os.path.isfile(drv):
        raise FileNotFoundError(f"Missing MicroPython WASI driver: {drv}")
    parts.append('#include "src/wasm_mpy_driver.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)

    # Preprocess single bundle with ZCC --pp-only -DNO_QSTR to capture ALL macro-generated QSTRs (zero GCC)
    bundle_rel = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    gen_rel = os.path.relpath(gen_root, REPO_ROOT).replace("\\", "/")
    mpy_rel = os.path.relpath(mpy_dir, REPO_ROOT).replace("\\", "/")
    pp_res = subprocess.run(
        [ZCC_BIN, "--pp-only", "-DNO_QSTR", f"-I{gen_rel}", f"-I{mpy_rel}", "-Isrc", "-I.", bundle_rel],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=90,
        check=True,
    )
    qstrs = set(re.findall(r"\bMP_QSTR_([A-Za-z0-9_]+)\b", pp_res.stdout))

    q_in = os.path.join(genhdr_dir, "qstrdefs.preprocessed.h")
    with open(q_in, "w", encoding="utf-8", newline="\n") as f:
        f.write("QCFG(BYTES_IN_LEN, 1)\nQCFG(BYTES_IN_HASH, 2)\n")
        with open(os.path.join(mpy_dir, "py", "qstrdefs.h"), "r", encoding="utf-8") as qf:
            for line in qf:
                m = re.match(r"^\s*Q\(([^)]*)\)", line)
                if m:
                    f.write(f"Q({m.group(1)})\n")
        for extra in SPECIAL_PRE_QSTRS:
            f.write(f"Q({extra})\n")
        for q in sorted(qstrs):
            if q in SPECIAL_QSTR_ENCODED_NAMES:
                continue
            f.write(f"Q({q})\n")

    r_q = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "makeqstrdata.py"), q_in],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(qgen_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(r_q.stdout)

    return bundle_path, len(all_c), len(qstrs), len(mod_regs)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone MicroPython v1.24.1 WASI module via zcc.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--probe-wasm-selfcompile", action="store_true")
    args = parser.parse_args()

    node_bin = find_node_bin()
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")

    bundle_path, num_c, num_qstrs, num_mods = write_bundle_and_genhdr(args.bundle)
    bundle_rel = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    out_rel = os.path.relpath(args.output, REPO_ROOT).replace("\\", "/")
    gen_rel = os.path.relpath(MPY_GENHDR_DIR, REPO_ROOT).replace("\\", "/")
    mpy_rel = os.path.relpath(MPY_DIR, REPO_ROOT).replace("\\", "/")
    bundle_sz = os.path.getsize(bundle_path)
    print(
        f"[BUNDLE] {bundle_rel}: {bundle_sz} bytes ({num_c} upstream MicroPython C modules, {num_qstrs} QSTRs, {num_mods} builtin modules)",
        flush=True,
    )

    cmd = [
        ZCC_BIN,
        "--target=wasm32",
        f"-I{gen_rel}",
        f"-I{mpy_rel}",
        "-Isrc",
        "-I.",
        bundle_rel,
        "-o",
        out_rel,
    ]
    print(f"[BUILD] {' '.join(cmd)}", flush=True)
    res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stdout + res.stderr)
        return res.returncode

    info = inspect_wasm_v8(node_bin, args.output)
    sha = sha256_file(args.output)
    print(f"[WASM] {args.output}: {info['bytes']} bytes, SHA256={sha}", flush=True)
    print(f"[V8] valid={info['valid']} compiled={info['compiled']} sections={info['sectionIds']} err={info['err']}", flush=True)
    non_wasi = [imp for imp in info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
    print(f"[IMPORTS] count={len(info['imports'])} non_wasi={non_wasi}", flush=True)
    if not info["valid"] or not info["compiled"] or non_wasi or 13 not in info["sectionIds"]:
        return 1

    if args.probe_wasm_selfcompile:
        if not os.path.isfile(ZCC_WASM):
            raise RuntimeError("build_wasm/zcc.wasm missing; run `python3 tests/test_wasm_wasi_selfhost.py` first.")
        probe_out = os.path.join(BUILD_WASM, "micropython_inwasi_probe.wasm")
        probe_rel = os.path.relpath(probe_out, REPO_ROOT).replace("\\", "/")
        wasm_cmd = [
            "zcc.wasm",
            "--target=wasm32",
            f"-I{gen_rel}",
            f"-I{mpy_rel}",
            "-Isrc",
            "-I.",
            bundle_rel,
            "-o",
            probe_rel,
        ]
        print(f"[PROBE-WASM] Invoking build_wasm/zcc.wasm on {bundle_rel}...", flush=True)
        try:
            pres = run_node_wasi(node_bin, ZCC_WASM, wasm_cmd, timeout=300)
            if pres.returncode == 0 and os.path.isfile(probe_out):
                pinfo = inspect_wasm_v8(node_bin, probe_out)
                psha = sha256_file(probe_out)
                p_non_wasi = [imp for imp in pinfo["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
                exec_res = run_node_wasi(
                    node_bin,
                    probe_out,
                    ["micropython.wasm", "-c", "print((2**64) + 1)"],
                    timeout=60,
                )
                exec_ok = (exec_res.returncode == 0 and exec_res.stdout.strip() == "18446744073709551617")
                print(
                    f"[PROBE-WASM] rc=0 bytes={pinfo['bytes']} valid={pinfo['valid']} "
                    f"compiled={pinfo['compiled']} non_wasi={p_non_wasi} exec_ok={exec_ok} "
                    f"SHA256={psha} byte_identical={psha == sha}",
                    flush=True,
                )
                if not pinfo["valid"] or not pinfo["compiled"] or p_non_wasi or not exec_ok:
                    return 1
            else:
                print(f"[PROBE-WASM] rc={pres.returncode} err={(pres.stdout + pres.stderr)[:400]}", flush=True)
                return 1
        finally:
            if os.path.exists(probe_out):
                os.remove(probe_out)

    print("[PASS] Standalone build_wasm/micropython.wasm validated with zero env.* imports!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
