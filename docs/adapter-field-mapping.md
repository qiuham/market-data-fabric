# Adapter Field Mapping

Provider field mapping belongs inside each adapter directory, not in `md-service`.

## Location

```text
libs/md-adapters/{asset_class}/{provider}/
  include/md/adapters/{provider}/
    *_field_mapper.hpp
    *_normalizer.hpp
  src/
    *_field_mapper.cpp
    *_normalizer.cpp
  schema/
    *.mapping.yaml
  docs/
    field_mapping.md
  tests/
    *_field_mapper_test.cpp
```

## Flow

```text
vendor bytes / SDK callback
  -> provider raw type
  -> provider field mapper
  -> md::core event
```

## Mapping Context

Field mappers should use a mapping context for source IDs, venue IDs, symbol maps, precision maps, calendars, and clocks. The service layer should create the context, but it should not know provider field details.
