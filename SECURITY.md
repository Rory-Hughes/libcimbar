# Security Policy

This fork is being prepared for security review and for possible use as an optical transport component in an air-gapped device. The upstream project describes libcimbar as experimental. Do not treat this fork as production-hardened until the audit gates in `docs/security/AUDIT_PLAN.md` have been completed.

## Supported branches

| Branch | Purpose | Security support |
|---|---|---|
| `audit-baseline-681e18e` | Frozen upstream reference at commit `681e18eb61a059f4a796bc6ef097d24b45c430eb` | No fixes; reference only |
| `hardening-scaffold` | Audit documentation, test scaffolding, and proposed hardening work | Active development |
| `master` | Fork of upstream | Tracks project owner decisions |

## Reporting a vulnerability

Do not publish exploit details, crash inputs, or proof-of-concept code in a public issue.

Preferred process:

1. Use GitHub's private **Report a vulnerability** feature for this repository when available.
2. Include the affected commit, platform, build flags, sanitizer output, reproduction steps, and impact assessment.
3. Attach the smallest reproducer that demonstrates the issue.
4. State whether the issue also appears to affect `sz3/libcimbar` upstream.

If private reporting is unavailable, contact the repository owner through GitHub before sharing technical details publicly.

## Scope priorities

Highest-priority reports include:

- Memory corruption reachable from image or frame input.
- Unbounded memory, CPU, disk, stream, or block-state growth.
- Cross-transfer state confusion.
- Unsafe decompression or output-file behaviour.
- Decoder sandbox escape.
- Unexpected data crossing the decoder-to-secure-core boundary.
- Build, dependency, or release-chain compromise.

## Disclosure handling

Security findings will be tracked using `docs/security/FINDING_TEMPLATE.md`. Potentially exploitable upstream findings should be coordinated privately with the upstream maintainer before public disclosure.
