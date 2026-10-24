# Fixture JSON schema 1

All properties below are required. Unknown and duplicate properties are rejected. The UTF-8 document limit is 4 MiB and nesting is limited to 16 levels.

```json
{
  "schema": 1,
  "name": "Mouse step",
  "speed": "full",
  "vid": 65535,
  "pid": 16,
  "input_bytes": 7,
  "report_descriptor": "05 01 ...",
  "reports": [
    { "at_us": 0, "hex": "00 00 00 00 00 00 00" },
    { "at_us": 10000, "hex": "00 C8 00 00 00 00 00" }
  ]
}
```

`report_descriptor` in this abbreviated illustration must be replaced by complete bytes; see Examples. Name length is 1–120 UTF-8 bytes; VID/PID are nonzero 16-bit integers. Descriptor limit: 4096 bytes. Hex accepts complete pairs separated by spaces, newlines or hyphens. `input_bytes` is 1–64 and includes report ID, if any. Every declared HID input report must have this fixed size. The model checks item bounds, collections, report count/size, IDs and global-stack balance; it is not a complete HID semantic verifier.

At most 10000 reports are allowed. `at_us` is nondecreasing elapsed microseconds, at most 3600000000. Input CI Runner schedules these best effort; it cannot guarantee device polling or application delivery at that instant. An empty reports array is valid for descriptor-only validation. No script execution, external paths, networking or file references are supported by the format.
