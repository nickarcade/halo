# 📜 Calling Convention ABI, Register Arguments & Verification Commands

This document provides a comprehensive reference for the calling conventions, custom register argument (`@<reg>`) subsystem, forward/reverse thunk architectures, and all associated verification and audit commands in the Halo CE Xbox (`cachebeta.xbe` build 2276) codebase.

---

## 🏛️ Calling Conventions Present in the Xbox Binary

The Halo CE Xbox debug executable combines multiple x86 calling conventions:

1. **`cdecl`**:
   - Standard C convention.
   - Caller pushes arguments right-to-left and cleans up the stack (`add esp, N`).
   - First `PUSH` in assembly corresponds to the last C argument.

2. **`stdcall` (`__stdcall`)**:
   - Used extensively by XDK, Direct3D 8, and WinSock APIs.
   - Arguments pushed right-to-left, but **callee** cleans up the stack via `ret N`.
   - Function pointer casts to standard functions must include `__stdcall` to avoid stack pointer drift.

3. **`fastcall` (`__fastcall`)**:
   - First two arguments passed in `ECX` and `EDX`.
   - Remaining arguments pushed right-to-left; callee cleans up stack.

4. **`thiscall`**:
   - Member function calls where `this` is passed in `ECX`.
   - Callee cleans up stack arguments via `ret N`.

5. **Custom Register Arguments (`@<reg>`)**:
   - Hand-optimized or compiler-inlined functions where one or more parameters are passed in arbitrary CPU registers (`EAX`, `ECX`, `EDX`, `EBX`, `ESI`, `EDI`, or sub-registers `AX`, `CX`, `DX`, `BX`, `SI`, `DI`, `AL`, `AH`, `BL`, `BH`, `CL`, `CH`, `DL`, `DH`).
   - Annotated explicitly in [`kb.json`](file:///storage/1F34-EBBE/halo/kb.json) using parameter annotations (e.g. `int handle@<eax>`, `void *buffer@<edi>`).
   - Reversible thunks bridge these functions seamlessly to and from standard C.

---

## 📂 Core Subsystem Files

The following files govern the Calling Convention ABI and Register Argument verification infrastructure:

| File Path | Description |
| :--- | :--- |
| [`kb.json`](file:///storage/1F34-EBBE/halo/kb.json) | Central knowledge base containing function signatures, memory addresses, ported status, and `@<reg>` annotations. |
| [`tools/kb_reg_baseline.json`](file:///storage/1F34-EBBE/halo/tools/kb_reg_baseline.json) | Authoritative baseline of all 1,047 verified `@<reg>` register-argument functions enforcing the immutability rule. |
| [`tools/audit/extract_reg_args.py`](file:///storage/1F34-EBBE/halo/tools/audit/extract_reg_args.py) | Tool to extract, audit, and compare register argument signatures against baseline caches. |
| [`tools/audit/audit_reg_abi.py`](file:///storage/1F34-EBBE/halo/tools/audit/audit_reg_abi.py) | Audits register-argument ABI risks and caller instruction setup against binary disassembly. |
| [`tools/audit/check_callee_reg_args.py`](file:///storage/1F34-EBBE/halo/tools/audit/check_callee_reg_args.py) | Scans ported C source for calls to original functions that pass implicit register arguments not yet annotated in `kb.json`. |
| [`tools/audit/check_const_reg_args.py`](file:///storage/1F34-EBBE/halo/tools/audit/check_const_reg_args.py) | Audits lifted C code for literal constants passed where the original pushes a register parameter. |
| [`tools/audit/dump_caller_regsetup.py`](file:///storage/1F34-EBBE/halo/tools/audit/dump_caller_regsetup.py) | Dumps assembly instruction sequences preceding every original `CALL` site to a callee to prove register inputs. |
| [`tools/build/patch.py`](file:///storage/1F34-EBBE/halo/tools/build/patch.py) | Binary patcher that generates and verifies forward and reverse thunks bridging `@<reg>` calls and standard C. |
| [`tools/hooks/pre-commit-reg-baseline-drift.sh`](file:///storage/1F34-EBBE/halo/tools/hooks/pre-commit-reg-baseline-drift.sh) | Git pre-commit hook preventing accidental drift or removal of register argument annotations. |
| [`docs/references/abi-and-calling-conventions.md`](file:///storage/1F34-EBBE/halo/docs/references/abi-and-calling-conventions.md) | Architectural guidelines on ABI conventions and `@<reg>` thunk principles. |

---

## 🛠️ Verification & Operational Commands

All commands below must be prefixed with `rtk` per repository doctrine.

### 1. Verify Register Arguments Against Baseline (Zero Drift Gate)
Verifies that no `@<reg>` annotation in `kb.json` has drifted, been deleted, or corrupted against `tools/kb_reg_baseline.json`:
```bash
rtk python3 tools/audit/extract_reg_args.py --check
```
*Expected Result:* `Check results: 1047 OK, 0 drift, 0 missing, 0 stale. No drift detected.`

### 2. Update Register Baseline
When new register-argument functions are newly proven and added to `kb.json`, update the baseline snapshot:
```bash
rtk python3 tools/audit/extract_reg_args.py --update
```

### 3. Test & Verify Thunk Generation
Executes the comprehensive patcher self-test suite covering exact-byte reverse thunk emission, sub-register widening, rotation cycles, and deactivation stubs for all 1,047 registered `@<reg>` functions:
```bash
rtk python3 tools/build/patch.py --test-thunks
```
*Expected Result:* `All reverse thunk self-tests passed.`

### 4. Audit Callee Register Arguments in Ported Code
Scans ported C translation units for calls to unported functions that read registers before writing them without `@<reg>` annotations:
```bash
rtk python3 tools/audit/check_callee_reg_args.py --check
```

### 5. Inspect Caller Register Setup for a Callee
Inspects the preceding 12 assembly instructions at every callsite of a target address in `cachebeta.xbe` to verify which registers are loaded:
```bash
rtk python3 tools/audit/dump_caller_regsetup.py 0x1b5c90
```

### 6. Audit Register ABI of a Function
Audits caller evidence and registers passed to a specific function:
```bash
rtk python3 tools/audit/audit_reg_abi.py --target 0x1812b0
```

### 7. Audit Constant Register Arguments
Audits lifted C code for literal constants passed where a register argument is expected:
```bash
rtk python3 tools/audit/check_const_reg_args.py --check
```

---

## 🔄 Thunk Architecture & Immutability Rules

### Forward Thunks (Ported C -> Original Binary)
When ported C code calls an unported original Xbox function that expects arguments in registers:
- In C, call the function by its declared prototype with standard arguments.
- The build generates a forward thunk that moves standard stack arguments into the appropriate registers (`MOV EAX, [ESP+4]`, etc.) before jumping to the original binary code.
- **Rule**: Never use raw function pointer casts or manual inline assembly to call register-arg functions.

### Reverse Thunks (Original Binary -> Ported C)
When original Xbox code calls a reimplemented C function that was originally register-arg:
- The build redirects the original address to a reverse thunk.
- The reverse thunk pushes register arguments onto the stack in standard `cdecl` order.
- Your ported C function is written as standard, clean C without assembly or manual register handling.

### The Immutability Rule
`@<reg>` annotations describe physical properties of the original compiled binary:
- **Never remove `@<reg>` annotations**, even when a function is 100% ported and all callers are ported.
- Removing an annotation causes a hard build failure at the `extract_reg_args.py` gate.
- Renaming functions or refining types is fully permitted as long as the `@<reg>` slot is preserved.
