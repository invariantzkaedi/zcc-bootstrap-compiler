# Batch 7 Bisection Results

## Bad function(s) identified
- `next_token` (Group A)
- `read_char` (Group A)
- `read_escape` (Group A)
- `node_name` (Group B)

## Safe functions confirmed clean
- `peek_token`
- `peek_char`
- `is_type_token`
- `lookup_keyword`
- `lookup_keyword_fallback`
- `expect`
- `node_ptr_elem_size`
- `ptr_elem_size`
- `get_member_size`

## Bisect log
- Round 1 Group A (`next_token`, `peek_token`, `peek_char`, `read_char`, `read_escape`, `is_type_token`, `lookup_keyword`): BAD
- Round 1 Group B (`lookup_keyword_fallback`, `expect`, `node_name`, `node_ptr_elem_size`, `ptr_elem_size`, `get_member_size`): BAD
- Round 2 Subgroup A1 (`next_token`, `peek_token`, `peek_char`, `read_char`): BAD
- Round 2 Subgroup A2 (`read_escape`, `is_type_token`, `lookup_keyword`): BAD
- Round 2 Subgroup B1 (`lookup_keyword_fallback`, `expect`, `node_name`): BAD
- Round 2 Subgroup B2 (`node_ptr_elem_size`, `ptr_elem_size`, `get_member_size`): CLEAN
- Round 3 Subgroup A1a (`next_token`, `peek_token`): BAD
- Round 3 Subgroup A1a1 (`next_token`): BAD
- Round 3 Subgroup A1a2 (`peek_token`): CLEAN
- Round 3 Subgroup A1b (`peek_char`, `read_char`): BAD
- Round 3 Subgroup A1b1 (`peek_char`): CLEAN
- Round 3 Subgroup A2a (`read_escape`): BAD
- Round 3 Subgroup A2b (`is_type_token`, `lookup_keyword`): CLEAN
- Round 3 Subgroup B1a (`lookup_keyword_fallback`): CLEAN
- Round 3 Subgroup B1b1 (`expect`): CLEAN
