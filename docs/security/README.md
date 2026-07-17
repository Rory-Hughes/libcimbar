# Libcimbar Security Hardening Workspace

This directory contains the working security documentation for the hardened libcimbar transport profile intended for the Air-Gapped Secure Companion project.

## Baseline

The frozen audit reference is:

- Repository: `Rory-Hughes/libcimbar`
- Branch: `audit-baseline-681e18e`
- Commit: `681e18eb61a059f4a796bc6ef097d24b45c430eb`
- Upstream: `sz3/libcimbar`

The frozen branch must not receive fixes or documentation edits. All audit and hardening work belongs on a separate working branch.

## Documents

- [`BASELINE.md`](BASELINE.md): exact audit target, current build facts, dependency inventory, and preliminary observations.
- [`THREAT_MODEL.md`](THREAT_MODEL.md): assets, attackers, trust boundaries, security goals, and non-goals.
- [`ATTACK_SURFACE.md`](ATTACK_SURFACE.md): decoder data flow and component review map.
- [`HARDENED_PROFILE.md`](HARDENED_PROFILE.md): restricted product profile and proposed resource limits.
- [`AUDIT_PLAN.md`](AUDIT_PLAN.md): phased audit, remediation, validation, and release gates.
- [`TEST_PLAN.md`](TEST_PLAN.md): sanitizer, static-analysis, fuzzing, property-test, and adversarial-lab plan.
- [`FINDING_TEMPLATE.md`](FINDING_TEMPLATE.md): required format for recording security findings.
- [`DECISION_LOG.md`](DECISION_LOG.md): architecture and audit decisions.

## Working rules

1. Treat every frame, symbol, metadata field, block identifier, object length, compression header, and filename as attacker-controlled.
2. Distinguish an audit observation from a confirmed vulnerability.
3. Add a regression test for every fixed security defect.
4. Preserve exact commit, compiler, dependency, and build information for every test result.
5. Never add secrets, private crash artifacts, or undisclosed exploit details to the public repository.
6. Keep the production transport profile smaller than the generic upstream file-transfer application.
7. Do not allow a successful barcode reconstruction to imply message, wallet, or firmware authenticity.

## Intended end state

The product-facing component should accept a fixed raw camera-frame format and return one bounded opaque byte object. It should not possess keys, parse application data, choose filenames, write arbitrary files, decompress message content, or access the network.
