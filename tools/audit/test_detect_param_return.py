"""Tests for detect_param_return.py.

Every case feeds hand-assembled machine code straight to `classify_bytes`, so
no XBE, kb.json or Ghidra is needed.

Run:
    .venv/bin/python -m pytest -q -p no:cacheprovider tools/audit/test_detect_param_return.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import detect_param_return as dpr  # noqa: E402

BASE = 0x10000


def run(hexbytes, base=BASE):
    return dpr.classify_bytes(bytes.fromhex(hexbytes.replace(" ", "")), base)


def def_kinds(r):
    return sorted({d["kind"] for e in r["exits"] for d in e["defs"]})


class ClassifyTests(unittest.TestCase):
    def test_ebp_frame_param_return(self):
        # push ebp; mov ebp,esp; mov eax,[ebp+0x10]; mov dword [eax],0;
        # pop ebp; ret
        r = run("55 8B EC 8B 45 10 C7 00 00 00 00 00 5D C3")
        self.assertTrue(r["ebp_frame"])
        self.assertEqual(r["class"], "param_return")
        self.assertEqual(r["param"], 2)
        self.assertEqual(r["confidence"], "medium")   # EAX used as the pointer
        self.assertEqual(r["n_rets"], 1)

    def test_ebp_frame_load_only_feeds_ret_is_high(self):
        # push ebp; mov ebp,esp; mov eax,[ebp+8]; pop ebp; ret
        r = run("55 8B EC 8B 45 08 5D C3")
        self.assertEqual((r["class"], r["param"], r["confidence"]),
                         ("param_return", 0, "high"))

    def test_esp_frame_no_push(self):
        # mov eax,[esp+0xc]; ret   -> depth 0: (0xc - 4) / 4 = param 2
        r = run("8B 44 24 0C C3")
        self.assertFalse(r["ebp_frame"])
        self.assertEqual((r["class"], r["param"]), ("param_return", 2))

    def test_esp_frame_push_shifts_delta(self):
        # push esi; mov eax,[esp+0xc]; pop esi; ret
        # depth 4 at the load: (0xc - 4 - 4) / 4 = param 1, not 2
        r = run("56 8B 44 24 0C 5E C3")
        self.assertEqual((r["class"], r["param"]), ("param_return", 1))

    def test_esp_frame_sub_esp_shifts_delta(self):
        # sub esp,8; mov eax,[esp+0x10]; add esp,8; ret
        # depth 8: (0x10 - 8 - 4) / 4 = param 1
        r = run("83 EC 08 8B 44 24 10 83 C4 08 C3")
        self.assertEqual((r["class"], r["param"]), ("param_return", 1))

    def test_xor_eax_is_not_param(self):
        # xor eax,eax; ret
        r = run("33 C0 C3")
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["const"])

    def test_call_before_ret_is_not_param(self):
        # mov eax,[esp+4]; call <next>; ret  -- EAX comes from the callee
        r = run("8B 44 24 04 E8 00 00 00 00 C3")
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["call_result"])

    def test_two_rets_different_params_is_partial(self):
        # 0: mov eax,[esp+4]; 4: test eax,eax; 6: je 9; 8: ret;
        # 9: mov eax,[esp+8]; 13: ret
        r = run("8B 44 24 04 85 C0 74 01 C3 8B 44 24 08 C3")
        self.assertEqual(r["n_rets"], 2)
        self.assertEqual(r["class"], "partial")
        self.assertEqual(r["params_seen"], [0, 1])
        self.assertIsNone(r["param"])

    def test_param_on_one_path_const_on_other_is_partial(self):
        # 0: mov eax,[esp+4]; 4: test eax,eax; 6: jne 10; 8: xor eax,eax;
        # 10: ret     -- two paths reach the one ret
        r = run("8B 44 24 04 85 C0 75 02 33 C0 C3")
        self.assertEqual(r["n_rets"], 1)
        self.assertEqual(r["class"], "partial")
        self.assertEqual(def_kinds(r), ["const", "param"])

    def test_ebp_local_is_not_param(self):
        # push ebp; mov ebp,esp; push ecx; mov eax,[ebp-4]; mov esp,ebp;
        # pop ebp; ret
        r = run("55 8B EC 51 8B 45 FC 8B E5 5D C3")
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["local_load"])

    def test_esp_local_is_not_param(self):
        # push ecx; mov eax,[esp]; pop ecx; ret   -> depth 4, disp 0 = a local
        r = run("51 8B 04 24 59 C3")
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["local_load"])

    def test_ebp_load_without_frame_is_not_param(self):
        # mov eax,[ebp+8]; ret  -- EBP is a general register here (FPO)
        r = run("8B 45 08 C3")
        self.assertFalse(r["ebp_frame"])
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["ebp_nonframe_load"])

    def test_common_epilogue_via_jmp(self):
        # mov eax,[esp+4]; jmp 6; ret   -- the load sits in another block
        r = run("8B 44 24 04 EB 00 C3")
        self.assertEqual((r["class"], r["param"]), ("param_return", 0))

    def test_esp_load_before_frame_teardown_is_unknown_frame(self):
        # push ebp; mov ebp,esp; mov eax,[esp+0xc]; mov esp,ebp; pop ebp; ret
        # `mov esp, ebp` sits between the load and ret: depth not recoverable
        r = run("55 8B EC 8B 44 24 0C 8B E5 5D C3")
        self.assertEqual(r["class"], "unknown_frame")
        self.assertIsNone(r["confidence"])

    def test_param_slot_reused_as_local(self):
        # push ebp; mov ebp,esp; mov [ebp+8],ecx; mov eax,[ebp+8]; pop ebp; ret
        r = run("55 8B EC 89 4D 08 8B 45 08 5D C3")
        self.assertEqual(r["class"], "slot_reused")
        self.assertEqual(r["slot_written"], "store")

    def test_param_slot_address_taken(self):
        # push ebp; mov ebp,esp; lea ecx,[ebp+8]; push ecx; pop ecx;
        # mov eax,[ebp+8]; pop ebp; ret
        r = run("55 8B EC 8D 4D 08 51 59 8B 45 08 5D C3")
        self.assertEqual(r["class"], "slot_reused")
        self.assertEqual(r["slot_written"], "address_taken")

    def test_x87_value_left_live_is_low_confidence(self):
        # push ebp; mov ebp,esp; mov eax,[ebp+8]; fld dword [eax]; pop ebp; ret
        # (a float-returning function that used param_1 as a pointer)
        r = run("55 8B EC 8B 45 08 D9 00 5D C3")
        self.assertEqual((r["class"], r["confidence"]), ("param_return", "low"))

    def test_tail_jump_exit_is_not_param(self):
        # mov eax,[esp+4]; jmp far outside the function
        r = run("8B 44 24 04 E9 00 10 00 00")
        self.assertEqual(r["n_tail_jumps"], 1)
        self.assertEqual(r["class"], "not_param")
        self.assertEqual(def_kinds(r), ["tail_jump"])

    def test_ret_n_stdcall(self):
        # mov eax,[esp+8]; ret 8   -> param 1
        r = run("8B 44 24 08 C2 08 00")
        self.assertEqual((r["class"], r["param"]), ("param_return", 1))

    def test_switch_table_targets_are_followed(self):
        # 0:  mov eax,[esp+4]
        # 4:  jmp dword ptr [eax*4 + table]       (table at BASE+24)
        # 11: mov eax,[esp+8]; 15: ret
        # 16: mov eax,[esp+8]; 20: ret
        # 21: int3 x3 ; 24: table {BASE+11, BASE+16}; 32: 0 (terminator)
        table = BASE + 24
        code = ("8B 44 24 04 FF 24 85" + table.to_bytes(4, "little").hex()
                + "8B 44 24 08 C3 8B 44 24 08 C3 CC CC CC"
                + (BASE + 11).to_bytes(4, "little").hex()
                + (BASE + 16).to_bytes(4, "little").hex() + "00000000")
        r = run(code)
        self.assertEqual(r["incomplete_cfg"], [])
        self.assertEqual(r["n_rets"], 2)
        self.assertEqual((r["class"], r["param"]), ("param_return", 1))

    def test_unresolved_indirect_jump_is_flagged(self):
        # mov eax,[esp+4]; jmp eax
        r = run("8B 44 24 04 FF E0")
        self.assertEqual(r["incomplete_cfg"], ["0x10004"])
        self.assertEqual(r["class"], "no_exit")


class DeclTests(unittest.TestCase):
    def test_register_params_are_not_stack_params(self):
        d = dpr.parse_decl("void FUN_0018d040(void *state @<eax>, float *value @<ecx>,"
                           " int16_t p1, int p2);")
        self.assertEqual(d["name"], "FUN_0018d040")
        self.assertEqual(d["ret"], "void")
        self.assertEqual([p["name"] for p in d["stack_params"]], ["p1", "p2"])

    def test_pointer_return_and_calling_convention(self):
        d = dpr.parse_decl("float * __cdecl vector3d_add(float *a, const float *b,"
                           " float *out);")
        self.assertEqual(d["ret"], "float *")
        self.assertEqual(d["stack_params"][2]["type"], "float *")
        self.assertEqual(d["stack_params"][2]["name"], "out")

    def test_function_pointer_param_and_varargs(self):
        d = dpr.parse_decl("void qsort(void *base, int (*cmp)(const void *, const void *),"
                           " ...);")
        self.assertEqual(d["name"], "qsort")
        self.assertTrue(d["varargs"])
        self.assertEqual(len(d["stack_params"]), 2)

    def test_void_param_list(self):
        d = dpr.parse_decl("void f(void);")
        self.assertEqual(d["params"], [])


if __name__ == "__main__":
    unittest.main()
