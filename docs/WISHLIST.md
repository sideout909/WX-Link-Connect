# WXLink Connect wishlist

This is an idea list, not a delivery commitment. Items here are not implemented and must not be presented as current firmware behavior.

## Owner-operated, privacy-reviewed submission relay

Explore an optional path for volunteers to submit a reviewed evidence bundle without putting the project owner's GitHub password, personal access token or other write credential on the device or in a volunteer's hands.

A possible design is a small owner-operated HTTPS intake service that validates bundle schema, checksums, size limits and privacy review metadata, then stores submissions in a private queue for owner review. If approved, the service could create a GitHub issue or commit using a narrowly scoped GitHub App installation token held only server-side. Tokens should be revocable and restricted to the one repository and required operation. No device would receive a repository write token.

Before considering implementation, define consent and disclosure, authentication or abuse controls, rate limits, retention/deletion, encryption, log redaction, review ownership, incident response and the exact contents made public. Keep local export and manual review available as the default fallback. Never auto-publish raw packets, nearby-device identifiers, credentials, NVS, owner logs or unreviewed captures.

A zero-server alternative is for volunteers to export and inspect a bundle, then manually upload it through GitHub's web interface. This avoids distributing the owner's credential but still requires review before repository inclusion.

**Current status:** no upload endpoint, cloud backend, GitHub API write, volunteer token or automatic submission exists in WXLink Connect. The current firmware remains local-capture-only.
