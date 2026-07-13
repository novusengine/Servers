# Server Tests

Unit tests run normally through `Server-Tests` when `Build UnitTests` is enabled.

PostgreSQL tests are hidden behind the `[.integration]` Catch2 tag. Set
`NOVUS_TEST_POSTGRES_CONNECTION` to a libpq keyword connection string for a maintenance database whose user may create and drop databases, then run:

```text
Server-Tests.exe "[.integration]"
```

DBController reconnect and `createIfMissing` cases additionally use
`NOVUS_TEST_POSTGRES_HOST`, `NOVUS_TEST_POSTGRES_PORT`, `NOVUS_TEST_POSTGRES_USER`, and `NOVUS_TEST_POSTGRES_PASSWORD`.

The integration fixture creates uniquely named `novus_server_test_*` databases and drops them after each case. Never point the variable at credentials that lack permission to create disposable databases or at a server where that naming prefix is not safe to use.
