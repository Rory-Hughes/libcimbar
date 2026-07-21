# Threat Model

## System role

Libcimbar is treated as an optical transport codec running in the untrusted Optical Interface Processor. It converts hostile camera input into a reconstructed byte object. It does not establish message authenticity or authorization.

## Trust boundaries

```text
Compromised networked companion
            |
      hostile display
            |
         camera
            |
  Optical Interface Processor
  - camera and frame processing
  - cimbar extraction and decoding
  - ECC and fountain reconstruction
  - no keys or plaintext
            |
      bounded typed IPC
            |
     Secure Core Processor
  - envelope validation
  - authentication and decryption
  - wallet and update policy
```

## Protected assets

The decoder must not gain access to:

- Messaging identity keys.
- Session and ratchet keys.
- Message plaintext.
- Wallet seeds or signing keys.
- Firmware authorization keys.
- Secure-core private memory.
- Trusted approval controls.

Assets inside the decoder domain that still require protection include:

- Process integrity.
- Availability within resource budgets.
- Transfer-state separation.
- Exact reconstructed output.
- IPC integrity.
- Diagnostic confidentiality.

## Threat actors

### Compromised companion

Can display arbitrary frames, vary timing, replay prior transfers, create malformed metadata, and observe user interaction.

### Malicious sender

Can produce validly encoded but adversarial payloads, inconsistent transfer sequences, extreme object sizes, duplicate blocks, and no-progress streams.

### Remote relay or observer

Can reorder, delay, replace, or suppress opaque objects before the companion renders them.

### Skilled local attacker

May attempt to exploit the decoder process, inspect writable storage, or use decoder compromise as a path toward the secure core.

### Supply-chain attacker

May modify dependencies, build inputs, release artifacts, compiler configuration, or vendored source.

## Primary attack classes

- Memory corruption in image, geometry, ECC, fountain, decompression, or output code.
- Integer overflow and truncation.
- Unbounded memory or CPU consumption.
- Persistent state growth across transfers.
- Transfer-state collision or confusion.
- Decompression bombs.
- Unsafe filename and filesystem operations.
- Race conditions in concurrent receivers or queues.
- Diagnostic leakage.
- Sandbox escape.
- Dependency or build compromise.

## Security goals

1. Invalid input results in bounded rejection, not undefined behaviour.
2. Memory, CPU, frame count, transfer duration, block count, and output length are explicitly bounded.
3. Only one configured transfer context is active in the initial product profile.
4. Conflicting metadata invalidates the transfer.
5. No partial object crosses IPC.
6. A complete object crosses IPC at most once.
7. The decoder cannot select an application action, filename, output directory, or payload interpretation.
8. Decoder restart clears all transfer state.
9. The production decoder has no network access and no secret access.
10. Secure-core authentication is required before application parsing or rendering.

## Non-goals

The decoder does not provide:

- End-to-end encryption.
- Message or sender authentication.
- Replay protection at the messaging protocol layer.
- Wallet transaction interpretation.
- Firmware signature verification.
- Metadata anonymity.
- Guaranteed availability against a malicious companion.

## Key security assumption

A successful cimbar reconstruction means only that a byte sequence was recovered from optical input. It does not mean the object is trusted, current, authorized, or safe to parse.
