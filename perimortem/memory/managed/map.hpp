// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/hash.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "perimortem/utility/pair.hpp"

namespace Perimortem::Memory::Managed {

// Arena backed scalar map for structured views that share one lifetime.
//
// Like the other managed structures only insert is supported since all produced
// data by `Managed` types are meant to be stable.
//
// Algorithm and storage is a trimmed down version of the more general Scalar
// version of `Dynamic::Map`.
template <typename key_type, typename value_type>
class Map {
 public:
  using Entry = Utility::Pair<key_type, value_type>;

  constexpr Map(Allocator::Arena& arena) : arena(arena) {}

  // A copy keeps the same allocation lifetime but has its own table. Sharing
  // buckets while copying size would let clearing one map invalidate the
  // other's entries without updating its bookkeeping.
  Map(const Map& rhs) : arena(rhs.arena) {
    if (rhs.is_empty()) {
      return;
    }

    ensure_capacity(rhs.get_size());
    for (Count i = 0; i < rhs.buffer.bucket_count; i++) {
      if (rhs.buffer.bucket_buffer[i]) {
        const auto& slot = rhs.buffer.slots_buffer[i];
        emplace_hashed(slot.hash, slot.entry.key, slot.entry.value);
      }
    }
  }

  auto ensure_capacity(Count items) -> void {
    if (buffer.bucket_buffer && items * 10 <= buffer.bucket_count * 9) {
      return;
    }

    Count bucket_count = buffer.bucket_count == 0 ? 2 : buffer.bucket_count * 2;
    while (items * 10 > bucket_count * 9) {
      bucket_count *= 2;
    }

    rehash(bucket_count);
  }

  auto clear() -> void {
    for (Count i = 0; i < buffer.bucket_count; i++) {
      buffer.bucket_buffer[i] = 0;
    }

    buffer.size = 0;
  }

  auto insert(const Entry& item) -> Entry* {
    return insert(item.key, item.value);
  }

  auto insert(const key_type& key, const value_type& value) -> Entry* {
    auto entry = find(key);
    if (entry) {
      (*entry).value = value;
      return &*entry;
    }

    ensure_capacity(buffer.size + 1);
    return emplace_hashed(get_hash(key), key, value);
  }

  // Adds the key only if it's not in the map already, but if it hits a conflict
  // then it launders the object avoiding constructors and destructors.
  auto launder(const key_type& key, const value_type& value) -> Entry* {
    auto entry = find(key);
    if (entry) {
      Core::Data::launder((*entry).value, value);
      return &*entry;
    }

    ensure_capacity(buffer.size + 1);
    return emplace_hashed(get_hash(key), key, value);
  }

  template <typename found_func, typename missing_func>
  constexpr auto
      visit(const key_type& key, found_func found, missing_func missing) const
      -> decltype(auto) {
    auto element = find(key);
    if (element) {
      return found((*element).value);
    } else {
      return missing();
    }
  }

  auto find(const key_type& key) -> Core::Option<Entry&> {
    Slot* slot = find_slot(key);
    if (slot == nullptr) {
      return {};
    }

    return slot->entry;
  }

  auto find(const key_type& key) const -> Core::Option<const Entry&> {
    const Slot* slot = find_slot(key);
    if (slot == nullptr) {
      return {};
    }

    return slot->entry;
  }

  auto contains(const key_type& key) const -> Bool { return bool(find(key)); }

  auto get_entry(Count index) -> Entry* {
    return const_cast<Entry*>(static_cast<const Map*>(this)->get_entry(index));
  }

  auto get_entry(Count index) const -> const Entry* {
    if (index >= buffer.size) {
      return nullptr;
    }

    for (Count bucket = 0; bucket < buffer.bucket_count; bucket++) {
      if (buffer.bucket_buffer[bucket] == 0) {
        continue;
      }

      if (index == 0) {
        return &buffer.slots_buffer[bucket].entry;
      }

      index--;
    }

    return nullptr;
  }

  auto at(const key_type& key) -> value_type& {
    auto entry = find(key);
    if (entry) {
      return (*entry).value;
    }

    return insert(key, value_type())->value;
  }

  auto operator[](const key_type& key) -> value_type& { return at(key); }

  auto get_size() const -> Count { return buffer.size; }
  auto get_capacity() const -> Count { return buffer.bucket_count; }
  auto is_empty() const -> Bool { return buffer.size == 0; }

 private:
  struct Slot {
    Entry entry;
    U32 hash = 0;
  };

  class Buffer {
   public:
    U8* bucket_buffer = nullptr;
    Slot* slots_buffer = nullptr;
    Count bucket_count = 0;
    Count size = 0;
  };

  auto find_slot(const key_type& key) -> Slot* {
    return const_cast<Slot*>(static_cast<const Map*>(this)->find_slot(key));
  }

  auto find_slot(const key_type& key) const -> const Slot* {
    if (buffer.size == 0) {
      return nullptr;
    }

    U32 hash = get_hash(key);
    Count bucket = bucket_index(hash);
    while (true) {
      if (buffer.bucket_buffer[bucket] == 0) {
        return nullptr;
      }

      if (buffer.slots_buffer[bucket].hash == hash &&
          buffer.slots_buffer[bucket].entry.key == key) {
        return buffer.slots_buffer + bucket;
      }

      bucket = (bucket + 1) & (buffer.bucket_count - 1);
    }
  }

  auto get_empty(U32 hash) -> Slot* {
    Count bucket = bucket_index(hash);
    while (buffer.bucket_buffer[bucket] != 0) {
      bucket = (bucket + 1) & (buffer.bucket_count - 1);
    }

    buffer.bucket_buffer[bucket] = 1;
    return buffer.slots_buffer + bucket;
  }

  auto emplace_hashed(U32 hash, const key_type& key, const value_type& value)
      -> Entry* {
    Slot* slot = get_empty(hash);
    slot->hash = hash;
    buffer.size++;
    new (&slot->entry, Core::Placement::Construct) Entry{key, value};
    return &slot->entry;
  }

  auto rehash(Count bucket_count) -> void {
    Buffer current = buffer;
    buffer = create_buffer(bucket_count);

    // A default buffer has no storage or entries to migrate. Keeping initial
    // allocation separate also makes the source storage invariant explicit.
    if (current.bucket_buffer == nullptr) {
      return;
    }

    for (Count i = 0; i < current.bucket_count; i++) {
      if (current.bucket_buffer[i] == 0) {
        continue;
      }

      emplace_hashed(
          current.slots_buffer[i].hash, current.slots_buffer[i].entry.key,
          current.slots_buffer[i].entry.value);
    }
  }

  auto create_buffer(Count bucket_count) -> Buffer {
    Buffer created;
    created.bucket_count = bucket_count;
    created.bucket_buffer = arena.allocate(bucket_count).get_data();
    created.slots_buffer = arena.reserve<Slot>(bucket_count).get_data();
    for (Count i = 0; i < bucket_count; i++) {
      created.bucket_buffer[i] = 0;
    }

    return created;
  }

  auto get_hash(const key_type& key) const -> U32 {
    return U32(Core::Hash(key).get_value());
  }

  auto bucket_index(U32 hash) const -> Count {
    return Count(hash & (buffer.bucket_count - 1));
  }

  Allocator::Arena& arena;
  Buffer buffer;
};

}  // namespace Perimortem::Memory::Managed
