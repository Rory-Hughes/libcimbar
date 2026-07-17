# Security Finding Template

Use one document or private tracker entry per finding.

## Identification

- **Finding ID:** LC-YYYY-NNN
- **Title:**
- **Status:** Draft / Confirmed / Remediating / Retest / Closed / Accepted Risk
- **Severity:** Critical / High / Medium / Low / Informational
- **Affected repository:**
- **Affected commit or range:**
- **Affected component and symbols:**
- **Reporter:**
- **Date reported:**
- **Upstream applicability:** Yes / No / Unknown
- **Disclosure status:** Private / Coordinating / Public

## Summary

Concise description of the defect and security consequence.

## Threat scenario

- Attacker capability required.
- Input or sequence required.
- Preconditions.
- User interaction required.
- Whether the issue crosses a trust boundary.

## Reproduction

- Build configuration.
- Platform.
- Exact steps.
- Minimal reproducer location.
- Sanitizer or diagnostic output.
- Reliability of reproduction.

Do not commit an undisclosed weaponized reproducer to the public repository.

## Technical analysis

- Root cause.
- Data flow from hostile input.
- Failed assumption or missing validation.
- Allocation, arithmetic, ownership, or lifecycle details.
- Why existing tests did not detect it.

## Impact

Assess separately:

- Confidentiality.
- Integrity.
- Availability.
- Persistence.
- Cross-transfer effects.
- Decoder-to-secure-core boundary effects.
- Product-profile relevance.
- Generic upstream relevance.

## Severity rationale

Explain exploitability, reliability, required privileges, affected configurations, and mitigating isolation.

## Remediation

- Proposed code change.
- Required architectural change.
- New limits or invariants.
- Compatibility impact.
- Upstream contribution plan.

## Verification

- Fix commit.
- Regression test.
- Fuzz corpus input identifier.
- Sanitizer retest.
- Cross-platform retest.
- Resource measurements before and after.
- Independent review result.

## Residual risk

Document remaining assumptions, incomplete coverage, or reasons for accepting risk.

## Disclosure timeline

Record private notification, maintainer acknowledgement, fix coordination, release, advisory, and public disclosure dates.
