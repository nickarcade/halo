# Handover

## Objective

Document the faithful calling convention for transport socket creation at
`FUN_00083930` (`0x83930`) without using an invalid VC71-only codegen cast.

## Current State

- Commit `fe790f689` removes the VC71-only `__fastcall` cast from
  `FUN_00083e20` (`0x83e20`). The call is now the ordinary C call
  `FUN_00083930(2, socktype, 0)` for every compiler.
- The original ABI is represented in `kb.json` as
  `af@<ecx>, type@<edx>, protocol@<eax>`; generated thunks bridge original
  register callers to the plain C implementation.
- Do not disturb unrelated current changes in `kb.json`,
  `src/halo/networking/network_game_globals.c`, or
  `src/halo/networking/network_messages.c`.

## Confirmed

- RXDK VC71 `CL.Exe` version 13.10.3077 rejects a hypothetical `__eaxcall`
  declaration (C2061/C2059).
- A VC71 `__fastcall` call puts its first two integer arguments in ECX/EDX
  and pushes its third argument. A probe casting a cdecl callee to
  `__fastcall` emitted that sequence, called the cdecl symbol, and returned
  without removing the pushed word. The removed cast was therefore
  ABI-invalid and could corrupt the stack in an MSVC-built path.
- The ordinary call is ABI-consistent with the current C definition
  `int FUN_00083930(int af, int type, int protocol)`.
- Fresh VC71 verification after removal: `FUN_00083e20` is 86.5% mnemonic
  match and 85.2% operand-normalized (155/155 instructions). The unsafe cast
  had reported 87.7% / 87.0%.

## Important Changes

- `src/halo/bungie_net/network/transport_endpoint_set_winsock.c`: removed the
  compiler conditional and `__fastcall` pointer cast at the socket-create
  call in `FUN_00083e20`.

## Validation

- Passed: `rtk python3 tools/build/build.py -q --target halo`.
- Passed: VC71 compiled the affected TU and scored `FUN_00083e20`.
- Passed: the commit hook's 110 quick Unicorn regression targets.
- `check_lift_hazards.py` still reports pre-existing hardcoded raw function
  pointer casts elsewhere in this source file (including line 295); they are
  not part of `fe790f689`.
- Fresh disassembly confirms all four original callers load `ECX=af`,
  `EDX=type`, and `EAX=protocol` immediately before the CALL, with no stack
  arguments.

## Uncertain / Risks

- The generic regression suite does not exercise socket creation or connect.

## Next Steps

1. Teach `vc71_verify` to model the EAX-register call if VC71 cannot express
   it, rather than changing runtime source solely for score.
2. Validate the recovered bridge on a real Xbox via XBDM, or xemu if hardware
   is unavailable, through a socket-create/connect path.

## Resume Prompt

`FUN_00083930` at `0x83930` is confirmed as `af@<ecx>, type@<edx>,
protocol@<eax>`; all original callers set those registers without stack args.
`fe790f689` correctly removed an ABI-invalid VC71 `__fastcall` cast. Keep the
plain C call and generated bridge; improve verifier modeling if byte scoring
needs it. Do not reintroduce function-pointer calling-convention casts.
