# Step 1 — Dependency-Ordered Summary and Design Gaps

You're right to stop and untangle the dependencies before writing more code. The previous message gave you files, but not the *order* they must be understood in, and not the honest accounting of where the design is weak. Let me fix both.

I'll do this in three parts:

1. **The dependency graph** — who depends on whom, in what order to think about them.
2. **Per-file contract** — what each file must satisfy for its dependents.
3. **Gaps and memory problems** — where the design is wrong, and what to do about it.

---

## Part 1: The Dependency Graph

```
                    ┌─────────────┐
                    │  types.h    │  (no deps — pure definitions)
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ row_ref.h   │  (deps: types.h)
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  value.h    │  (deps: types.h, row_ref.h)
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ column.h    │  (deps: types.h, row_ref.h)
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  table.h    │  (deps: types.h, row_ref.h, value.h, column.h)
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ runtime.h   │  (deps: table.h)
                    └─────────────┘
```

**This is the only order that works.** Each file depends on the ones above it, and only on those. If you find yourself wanting to include `table.h` from `column.h`, you've made a design mistake — stop and refactor.

The two `.cpp` files that matter (`column.cpp`, `table.cpp`) implement the headers. They can include anything, but they should only include their own header and the ones it depends on.

---

## Part 2: Per-File Contract — What Must Be True

For each file, I'll state:

- **What it provides** (the interface its dependents use)
- **What it must not do** (the constraints that keep the graph clean)
- **The invariants** (what must always be true)

### `types.h`

**Provides:**
- `RowIndex` (uint32), `Generation` (uint32), `RowId` (uint64)
- `ColumnType` enum
- `column_type_name(ColumnType) -> const char*`
- `column_type_size(ColumnType) -> size_t`

**Must not:**
- Include any other `lucid/` header.
- Define any class with members.
- Depend on any runtime state.

**Invariants:**
- `ColumnType` is a closed set. Adding a new kind is a breaking change.
- `RowIndex` index 0 is reserved for nil. This is a *global* invariant — every file that uses `RowIndex` must respect it.
- `column_type_size` returns 0 for variable-size types (`String`, `RowRef`). This is a convention, not a fact.

**Gap:** `RowId` (uint64) is defined but never used yet. It's a placeholder for the entity's global ID. It will be used in Step 5. **Either remove it now, or commit to using it.** I recommend keeping it, because the ECS will need it.

---

### `row_ref.h`

**Provides:**
- `RowRef` struct: `{ RowIndex index; Generation generation; }`
- `is_nil()`, `operator==`, `operator!=`
- `std::hash<RowRef>` specialization

**Must not:**
- Include `value.h` (would create a cycle).
- Include `column.h`, `table.h`, or `runtime.h`.
- Have any logic beyond comparison and nil-checking.

**Invariants:**
- `RowRef` is exactly 8 bytes (4 + 4). This is an ABI commitment.
- `RowRef{}` is nil. `index == 0` means nil. This is the *only* nil representation.
- Any `RowRef` with `index == 0` is invalid, regardless of `generation`.

**Gap:** The default constructor sets `index = 0, generation = 0`. But `is_nil()` only checks `index == 0`. That's correct, but it means `RowRef(0, 5)` is also nil. Is that intended? **Yes** — index 0 is reserved, so any ref with index 0 is nil. But this must be documented and enforced. A `Table` must never return a `RowRef` with `index == 0`.

---

### `value.h`

**Provides:**
- `Value` class: a tagged union of `Nil`, `Bool`, `Int`, `Float`, `String`, `RowRef`.
- `from_*` factories, `as_*` accessors, `operator==`.

**Must not:**
- Include `column.h` or `table.h`. (It depends only on `types.h` and `row_ref.h`.)
- Be used for storage. `Value` is a *transient* type for boundaries (host functions, editor).
- Be assumed to be cheap to copy.

**Invariants:**
- A `Value` is never in an "invalid" state. Every `Value` has a valid `Kind`.
- `as_*` throws if the kind doesn't match. No silent coercion.

**Gaps (significant):**

1. **`Value` is 48–56 bytes.** It carries a `std::string` (32 bytes on libstdc++) and a `RowRef` (8 bytes) even when it's a `bool`. This is wasteful, and it's the single biggest memory problem in Step 1.

2. **`Value` is used in the hot path** — `Table::get` and `Table::set` construct/destruct a `Value`, which means a `std::string` copy for string cells, and an 8-byte `RowRef` copy for everything else. For a render system iterating 10,000 sprites, this is 10,000 `Value` constructions per frame.

3. **`Value` is the reason the previous design was slow.** The typed API (`get_float32`, `set_int32`) is the escape hatch, but it's still going through `Column::get_*` which is fine, but `Table::get` and `Table::set` (the `Value`-based ones) are the problem.

**What to do about it:**

- **Keep `Value` for the editor and host function boundaries.** It's fine there.
- **Never use `Value` in a system's hot loop.** Systems use `Table::get_float32`, `Table::set_int32`, etc.
- **Consider a smaller `Value` variant later** (e.g., a `std::variant` with `std::string_view` for strings). But that's a Phase 8 optimization.
- **The most important change:** make the typed accessors on `Table` *the* API, and treat `Value` as a fallback. The render system should never call `Table::get(ref, "x")`. It should call `Table::get_float32(ref, "x")`.

**Also:** the previous design put `std::string s_` and `RowRef r_` *outside* the union. That means every `Value` allocates 32+8 bytes even for an int. **This is the correct design for a boundary type, and the wrong design for a storage type.** Since `Value` is boundary-only, it's OK. But be aware.

---

### `column.h`

**Provides:**
- `Column` class: a typed array of values with optional primary indexing.
- Typed getters/setters for all 14 column types.
- Primary lookup: `find_primary_bool`, `find_primary_int64`, etc.
- Row management: `ensure_size`, `resize`, `clear`.

**Must not:**
- Include `table.h` (would create a cycle).
- Include `value.h` (would create a cycle — `value.h` doesn't need `column.h`).
- Know about `RowRef`'s generation. `Column` operates on row *indices*, not `RowRef`s. Generation is the table's concern.
- Know about slots, free lists, or liveness. Those are the table's concern.

**Invariants:**
- A column's type is fixed at construction. `ColumnType type_` never changes.
- Only one storage vector is used, matching `type_`. The others are empty.
- Only one primary index is used, matching `type_`. The others are empty.
- `size()` returns the size of the *active* storage vector, not the largest one.
- `set_*(row, v)` grows the storage if `row >= size()`. This means setting row 100 on an empty column produces a column of size 101, with rows 0–99 default-initialized.

**Gaps (the big ones):**

1. **Memory: 14 vectors per column.** Every `Column` has 14 `std::vector` members, but only one is used. Each empty vector is 24 bytes (pointer + size + capacity). So every column wastes 13 × 24 = 312 bytes on empty vectors.

   For a table with 8 columns, that's 2.5 KB of wasted memory *per table*, regardless of row count. For 100 tables, 250 KB. Not catastrophic, but wasteful.

   **The fix: use a union of vectors, or a `std::variant`.** A union would reduce the per-column overhead to ~32 bytes (the largest vector) + a type tag. But unions with non-trivial types (`std::vector`) require manual construction/destruction, which is error-prone.

   **Better fix: store the data as a `std::vector<std::byte>` plus a typed view.** The typed view is a `reinterpret_cast` to the correct pointer type. This is what game engines do for component storage. It's more dangerous but more memory-efficient.

   **For Step 1, accept the 312-byte overhead per column.** It's not the bottleneck. Revisit in Phase 8.

2. **Memory: the primary index duplicates the key data.** For a `String` primary column, every key is stored twice: once in `string_data_`, once in `string_index_`. For 10,000 rows, that's 10,000 strings doubled.

   **The fix: store the index as `unordered_map<size_t, size_t>` (hash → row), and compare keys via the data array.** This is a "hash index" instead of a "value index." More complex, but no duplication.

   **For Step 1, accept the duplication.** It's correct, and it's simple. Optimize later.

3. **Correctness: `set_*` on a primary column doesn't check for duplicates consistently.** The previous `update_primary` scans the whole map to find the old entry for the row, then inserts the new one. If the new value already exists, it throws. But if the row already held that value, it should be a no-op, not a throw.

   **The fix:**
   ```cpp
   template <typename Map, typename Key>
   void update_primary(Map& map, size_t row, const Key& new_key) {
       // Find the old key for this row (if any).
       auto old_it = std::find_if(map.begin(), map.end(),
           [row](const auto& pair) { return pair.second == row; });

       if (old_it != map.end()) {
           if (old_it->first == new_key) return;  // no change
           map.erase(old_it);
       }

       auto [it, inserted] = map.emplace(new_key, row);
       if (!inserted) {
           throw std::runtime_error("Duplicate primary key");
       }
   }
   ```
   This is O(n) in the map size. For large tables, it's slow. **For Step 1, accept it.** Optimize in Phase 8 by storing the old key alongside the row.

4. **Correctness: `Column::remove_index_for_row` doesn't exist.** The previous `Table::remove` didn't clean up the primary index. This is a real bug.

   **The fix:** add `Column::remove_index_for_row(size_t row)`:
   ```cpp
   void Column::remove_index_for_row(std::size_t row) {
       if (!is_primary_) return;
       // Scan the active index, remove any entry pointing at `row`.
       switch (type_) {
           case ColumnType::Bool:    erase_by_value(bool_index_, row); break;
           case ColumnType::Int64:   erase_by_value(int64_index_, row); break;
           case ColumnType::Uint64:  erase_by_value(uint64_index_, row); break;
           case ColumnType::Float64: erase_by_value(double_index_, row); break;
           case ColumnType::String:  erase_by_value(string_index_, row); break;
           case ColumnType::RowRef:  erase_by_value(row_ref_index_, row); break;
           default: break;
       }
   }
   ```
   Then `Table::remove` calls this on the primary column before marking the slot dead.

5. **Design: `ensure_size` doesn't initialize new rows correctly for primary columns.** When a new row is added and the primary column grows, the new entry is default-initialized (e.g., `0` for uint64). If the primary column has a unique constraint, and `0` is already a key, this is a problem.

   **The fix:** `ensure_size` on a primary column should *not* add the new default value to the index. The index should only contain values that were explicitly set. This is what the previous design did (by not touching the index in `ensure_size`). **Confirm this is the behavior** and document it.

   Then `Table::add` must set the primary column's value *before* or *as part of* the row creation, so the index gets updated correctly.

   **Wait — there's a subtlety.** `Table::add` iterates columns in order and calls `set_*` on each. If the primary column is the first one, its `set_*` will add the index entry. If the primary column is somewhere else, the index entry is added when its turn comes. Either way, the row is valid only after all `set_*` calls complete. **This is fine**, but the order of columns matters for correctness during the add.

   **Actually, it doesn't matter**, because `Table::add` doesn't mark the slot as alive until after all columns are set. But it does — `Table::add` does `slots_[idx].alive = true` *before* setting the columns. So during the `set_*` calls, the slot is already alive, and a lookup by primary could find it mid-construction. **This is a bug.**

   **The fix:** mark the slot alive *after* all columns are set:
   ```cpp
   RowIndex idx = ...;  // get slot, but don't mark alive yet
   for (each column) set_*(idx, value);  // this builds the index
   slots_[idx].alive = true;             // now it's visible
   live_count_ += 1;
   ```
   But `Column::set_*` on a primary column adds to the index immediately, before the slot is alive. So a lookup between "index inserted" and "slot alive" would find a row that isn't valid yet. **This is a race condition in the design.**

   **The proper fix:** the primary index should map to `RowIndex`, and the *table* should validate the slot's liveness when returning a `RowRef`. So `find_by_primary` returns a `RowRef` with the current generation, and `is_valid` checks the slot. If the slot isn't alive, `is_valid` returns false. **This is already what the code does** — `find_by_primary` returns `RowRef(idx, slots_[idx].generation)`, and `is_valid` checks `slots_[ref.index].alive`.

   So the "race" is only within a single `Table::add` call, which is single-threaded. No other code can observe the intermediate state. **This is fine.** But it's worth documenting: `Table::add` is not reentrant, and the primary index may temporarily contain entries pointing at not-yet-alive slots during the call.

6. **Design: `Column` has 14 empty vectors.** This is the memory problem from #1. Accept it for now.

7. **Design: `Column::clear` clears the storage but doesn't reset `is_primary_`.** This is correct — the primary attribute is a schema property, not row data. But it means after a `clear`, the column is still primary. That's what we want. **Confirm this is the intent.**

8. **Design: `Column::resize` on a primary column rebuilds the index from scratch.** This is O(n) but correct. However, if the resize *shrinks* the column, the rows beyond the new size are dropped, and their index entries are removed by the rebuild. **This is correct**, but it means a row's `RowRef` becomes invalid after a shrink (because the underlying index is gone). The table's slot generation should also bump, but that's the table's job. **Document this.**

---

### `table.h`

**Provides:**
- `Table` class: a collection of `Column`s plus a slot array.
- `add(values) -> RowRef`
- `remove(ref)`, `clear()`, `count()`, `capacity()`, `is_valid(ref)`
- `get(ref, column)`, `set(ref, column, value)` (Value-based)
- `get_float32(ref, column)`, etc. (typed)
- `find_by_primary(column, value) -> RowRef`
- `each(fn)` iteration

**Must not:**
- Include `runtime.h` (would create a cycle).
- Know about the runtime's host functions.
- Know about the ECS or scenes.

**Invariants:**
- Slot 0 is reserved for nil. `slots_[0]` is never alive.
- A slot's generation is bumped every time the slot is reused. Generations are never 0 for live slots (they start at 1).
- A `RowRef` is valid iff its slot is alive and its generation matches the slot's.
- `count()` returns the number of live slots. `capacity()` returns `slots_.size()` (including dead slots).
- Rows never move between slots. A `RowRef`'s index is stable for the row's lifetime.
- The primary index maps *values* to *row indices*. It doesn't know about generations — that's the table's job.

**Gaps (the big ones):**

1. **Memory: the slot array grows without bound.** When a row is removed, its slot is marked dead and pushed onto `free_slots_`. But `slots_` itself never shrinks. After adding 1M rows and removing 999,999 of them, `slots_` still has 1M entries. **This is the slot-reuse memory problem from the grammar (§4.1.1d).**

   **The fix:**
   - `Table::remove` reuses slots (already does).
   - `Table::shrink()` releases the trailing dead slots. (Not implemented in Step 1.)
   - `Table::clear` resets the slot array to just slot 0, or keeps capacity. (The previous design kept capacity.)

   **For Step 1, accept the growth.** Implement `shrink` in Phase 8.

2. **Memory: `Column` storage arrays grow to `slots_.size()`.** When `Table::add` gets a new slot, it calls `ensure_columns_size(slots_.size())`, which grows every column's storage to the slot count. For a table with 8 columns and 1M slots, that's 8M values.

   **The fix:** use a sparse representation. But that's a major redesign, and it's not needed for Step 1. **Accept the growth.**

   **Note:** this is why `Table::clear` should shrink the column arrays. The previous design's `Table::clear` calls `col.clear()`, which resizes the storage vectors to 0. **This is correct** — the columns' `clear` releases the memory (or at least resizes to 0; whether it releases capacity depends on the vector's implementation, which typically doesn't shrink).

   Actually, `std::vector::clear()` doesn't release capacity. So after a `clear`, the column's storage has size 0 but capacity = old size. **This is a memory leak in the logical sense** — the memory is still held. For Step 1, accept it. For Phase 8, use `shrink_to_fit` or swap with an empty vector.

3. **Correctness: `Table::add` marks the slot alive before setting columns.** This is the race condition from `column.h` #5. **Accept it for Step 1**, but document that `Table::add` is not reentrant and the primary index may temporarily contain entries pointing at not-yet-alive slots.

   Actually, let me re-examine. The order in the previous `Table::add` is:
   ```
   1. get slot (from free list or new)
   2. mark slot alive
   3. ensure columns size
   4. set column values
   5. live_count_ += 1
   6. return RowRef
   ```
   Between steps 2 and 4, the slot is alive but the columns aren't set. If another thread reads the table, it could see a half-constructed row. **This is a threading bug, not a correctness bug in single-threaded code.**

   **For Step 1, single-threaded is fine.** For Phase 8, either use a lock or reorder to mark alive last.

4. **Correctness: `Table::remove` doesn't clean up the primary index.** This is the bug from `column.h` #4. **Fix it** by adding `Column::remove_index_for_row` and calling it from `Table::remove`.

5. **Correctness: `Table::clear` bumps generations of all slots.** This is what makes all `RowRef`s stale. **But it doesn't reset the free list correctly** — it clears the free list and pushes all slots (1..N) onto it. This means after a `clear`, the next `add` reuses slot 1, bumping its generation. **This is correct.**

   But the previous design also had `free_slots_.clear()` followed by a loop pushing all slots. This is correct but O(n). For a table with 1M slots, it's slow. **Accept for Step 1.**

6. **Design: `Table::find_by_primary` returns a `RowRef` with the slot's current generation.** This is correct — the caller gets a `RowRef` that's valid as long as the row is alive. **But the function doesn't check that the row is still alive after the index lookup.** If a row was removed but the index wasn't cleaned up (bug #4), the returned `RowRef` would have the slot's current generation, which might not match the row the index pointed at. **This is why bug #4 matters.**

   **Fix bug #4.** Then `find_by_primary` is correct.

7. **Design: `Table::get(ref, column)` returns `Value::nil()` if the ref is invalid.** This is convenient but hides bugs. **Consider throwing instead**, or at least logging. For Step 1, returning nil is fine, but the caller must check.

8. **Design: `Table::each` iterates slot order, not insertion order.** This is documented in the grammar (§4.1.1a). **Correct.**

---

### `runtime.h`

**Provides:**
- `Runtime` class: a collection of named tables plus host functions.
- `create_table(name)`, `get_table(name)`, `has_table(name)`, `each_table(fn)`
- `register_host_fn(name, fn)`, `has_host_fn(name)`, `call_host_fn(name, args)`

**Must not:**
- Know about the ECS, scenes, or the engine.
- Have any global state.

**Invariants:**
- Table names are unique. `create_table` throws on duplicate.
- `each_table` iterates in creation order (deterministic).
- Host functions are keyed by name. `register_host_fn` replaces an existing function.

**Gaps:**

1. **Memory: `table_order_` duplicates the keys of `tables_`.** For 100 tables, that's 100 strings duplicated. **Minor.** Accept it.

2. **Design: `Runtime` doesn't expose table removal.** If you want to delete a table, there's no API. **Add `remove_table(name)`** if needed later. For Step 1, not needed.

3. **Design: `Runtime` is not thread-safe.** Multiple threads calling `create_table` or `get_table` concurrently is a data race. **For Step 1, single-threaded.** Document it.

---

## Part 3: The Memory Problems, Ranked

Here are the memory issues in Step 1, ordered by how much they matter.

### Problem 1: `Value` is 48–56 bytes

Every `Value` carries a `std::string` (32 bytes) and a `RowRef` (8 bytes), even when it's a `bool`.

**Impact:** If `Value` is used in the hot path (it shouldn't be), it's a huge cost. If it's only used at boundaries, it's fine.

**Fix:** Don't use `Value` in systems. Use typed accessors. **This is a design discipline, not a code change.**

**Long-term fix:** A smaller `Value` (e.g., a variant with `std::string_view`). Phase 8.

### Problem 2: 14 vectors per `Column`

Every `Column` has 14 `std::vector` members, but only one is used. Each empty vector is 24 bytes, so every column wastes 312 bytes.

**Impact:** For a table with 8 columns, 2.5 KB wasted per table. For 100 tables, 250 KB. **Not catastrophic, but noticeable for many small tables.**

**Fix:** Use a union of vectors, or store the data as `std::vector<std::byte>` plus a typed view.

**For Step 1, accept it.** It's not the bottleneck. Revisit in Phase 8.

### Problem 3: Primary index duplicates key data

For a `String` primary column, every key is stored twice: once in the data array, once in the index.

**Impact:** For 10,000 rows with 20-byte keys, 200 KB duplicated.

**Fix:** Use a hash index (hash → row), and compare via the data array. More complex.

**For Step 1, accept it.**

### Problem 4: Slot array never shrinks

`Table::remove` marks slots dead and reuses them, but `slots_` never shrinks. After adding 1M rows and removing 999,999, `slots_` still has 1M entries.

**Impact:** For a table with high churn (add/remove cycles), memory grows to the peak. **Not a leak, but a high-water mark.**

**Fix:** `Table::shrink()` releases trailing dead slots. Phase 8.

### Problem 5: Column storage doesn't shrink on `clear`

`std::vector::clear()` sets size to 0 but keeps capacity. So after `Table::clear`, the columns' storage has size 0 but capacity = old size.

**Impact:** Memory is held until the table is destroyed. **Again, a high-water mark.**

**Fix:** Use `shrink_to_fit` or swap with an empty vector in `Table::clear`. Phase 8.

### Problem 6: `table_order_` duplicates keys

`Runtime::table_order_` duplicates the keys of `tables_`. **Minor.**

---

## Part 4: What to Actually Build

Given all of the above, here's the concrete plan.

### Build order (dependency-first)

1. **`types.h`** — pure definitions. No dependencies.
2. **`row_ref.h`** — `RowRef` struct, comparison, hash. Depends on `types.h`.
3. **`value.h`** + **`value.cpp`** — the transient value type. Depends on `types.h`, `row_ref.h`.
4. **`column.h`** + **`column.cpp`** — typed column with primary index. Depends on `types.h`, `row_ref.h`. **Add `remove_index_for_row`**.
5. **`table.h`** + **`table.cpp`** — the table. Depends on `types.h`, `row_ref.h`, `value.h`, `column.h`. **Fix `remove` to call `remove_index_for_row`**.
6. **`runtime.h`** + **`runtime.cpp`** — the runtime. Depends on `table.h`.

### Corrections to apply

1. **Add `Column::remove_index_for_row(size_t row)`** — clears the primary index entry pointing at `row`.
2. **Fix `Table::remove`** — call `remove_index_for_row` on the primary column before marking the slot dead.
3. **Fix `Column::update_primary`** — handle the "no change" case (setting a cell to its current value).
4. **Document `Table::add`'s non-reentrancy** — the slot is marked alive before columns are set.
5. **Document `Column::ensure_size`'s primary behavior** — new rows aren't indexed until their value is set.
6. **Document `RowRef`'s nil invariant** — index 0 is always nil.
7. **Remove `RowId` from `types.h`** if the ECS isn't using it yet, or commit to using it. (I recommend keeping it, because the ECS will need it.)

### What to accept for Step 1

- `Value` is large. Use typed accessors in hot paths.
- 14 vectors per column. 312 bytes wasted per column.
- Primary index duplicates key data.
- Slot array doesn't shrink.
- Column storage doesn't shrink on clear.
- `Table::add` is not reentrant.
- `Runtime` is not thread-safe.

**None of these are blocking.** They're all Phase 8 optimizations. Step 1 is about correctness and a clean API.

### What to test

- `RowRef`: default is nil, equality.
- `Column`: typed get/set, primary index, duplicate primary throws.
- `Table`: add/get, remove invalidates ref, slot reuse bumps generation, find_by_primary, iteration.
- `Table::remove` + `find_by_primary` → the primary index is cleaned up.
- `Runtime`: create/get table, duplicate throws, host functions.

The tests from the previous message cover most of these. **Add a test for `remove` + `find_by_primary`** to catch the bug:

```cpp
TEST_CASE("Table: remove cleans up primary index", "[table]") {
    Table t("Person");
    t.add_column("id", ColumnType::Uint64, true);

    RowRef a = t.add({ Value::from_uint64(1) });
    RowRef b = t.add({ Value::from_uint64(2) });

    t.remove(a);

    REQUIRE(t.find_by_primary("id", Value::from_uint64(1)).is_nil());
    REQUIRE_FALSE(t.find_by_primary("id", Value::from_uint64(2)).is_nil());
}
```

---

## Summary

**The dependency order is:**

```
types.h → row_ref.h → value.h → column.h → table.h → runtime.h
```

**Each file must satisfy:**

- `types.h` — definitions only.
- `row_ref.h` — comparison and hash.
- `value.h` — transient boundary type.
- `column.h` — typed storage + primary index.
- `table.h` — slots + columns + generation.
- `runtime.h` — named tables + host functions.

**The known gaps:**

1. `Column::remove_index_for_row` is missing — add it.
2. `Table::remove` doesn't clean up the primary index — fix it.
3. `Column::update_primary` doesn't handle "no change" — fix it.
4. `Value` is large — use typed accessors in hot paths.
5. 14 vectors per column — accept for now.
6. Primary index duplicates keys — accept for now.
7. Slot array doesn't shrink — accept for now.
8. Column storage doesn't shrink on clear — accept for now.
9. `Table::add` is not reentrant — document.
10. `Runtime` is not thread-safe — document.

**The build plan:**

1. Write the files in dependency order.
2. Apply the 3 correctness fixes (remove_index_for_row, Table::remove, update_primary).
3. Write the tests.
4. Build and run.
5. Fix compile errors.
6. Commit.

Once Step 1 passes, the foundation is solid. Step 2 (infrastructure) is quick. Step 3 (renderer) is the first visual milestone.

**The key discipline:** never use `Value` in a system's hot loop. Use `Table::get_float32`, `Table::set_int32`, etc. `Value` is for the editor and host function boundaries. This single rule avoids most of the memory cost of the `Value` design.