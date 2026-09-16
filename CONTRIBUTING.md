# Contributing

Contributions are welcome through GitHub issues and pull requests.

## Development setup

1. Clone with `git clone --recurse-submodules`.
2. Start the reference stack with `docker compose up --build`.
3. Run the SQL tests documented in `README.md`.
4. For web changes, run `npm ci` and `npm run build` in `web/`.

## Pull requests

- Keep changes focused and explain behavior changes.
- Add or update tests for SQL-visible behavior.
- Preserve compatibility with the supported PostgreSQL/PostGIS versions.
- Do not commit credentials, database volumes, build output or `node_modules`.
- Confirm `git diff --check` is clean before submitting.

By submitting a contribution, you agree that it is licensed under the Apache
License 2.0 used by this project.
