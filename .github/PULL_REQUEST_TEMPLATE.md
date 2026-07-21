## Summary

Describe the change and the security or audit objective it serves.

## Scope

- [ ] Documentation only
- [ ] Test or fuzzing infrastructure
- [ ] Decoder behaviour
- [ ] Dependency or build change
- [ ] Product-profile change
- [ ] Sandbox or IPC change

## Baseline and compatibility

- Base commit or branch:
- Upstream applicability:
- Behaviour or format compatibility impact:

## Security checklist

- [ ] Hostile-input assumptions are preserved.
- [ ] New or changed lengths use checked arithmetic.
- [ ] Memory, CPU, stream, block, frame, and output limits are explicit.
- [ ] No new filesystem, network, filename, decompression, or dynamic-loading surface is introduced without an approved decision record.
- [ ] No decoder output is treated as authenticated application content.
- [ ] Failure, timeout, cancellation, and restart paths clear state.
- [ ] Logs and diagnostics contain no secrets or uncontrolled input.
- [ ] A regression test covers the security property changed.
- [ ] Relevant fuzz target or corpus has been updated.
- [ ] Sanitizer and static-analysis results have been reviewed.

## Validation

List commands, platforms, test results, fuzz duration, and resource measurements.

## Findings and decisions

Link any finding record or ADR affected by this change.

## Disclosure

- [ ] This pull request contains no undisclosed exploit details or sensitive crash artifact.
