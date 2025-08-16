#include <iostream>
#include <libwavy/db/entry.hpp>
#include <nlohmann/json.hpp>

using namespace libwavy::db;
using namespace nlohmann;

struct dummy
{
  int hi;
};

// not necessary to implement this
auto operator<<(std::ostream& os, const dummy& e) -> std::ostream&
{
  return os << "{hi=" << e.hi << "}";
}

auto main() -> int
{
  try
  {
    std::filesystem::path db_dir = "./testdb";
    LMDBKV<dummy>         kv(db_dir);

    std::cout << "=== LMDBKV Demo ===" << std::endl;

    // --------------------
    // 1. put simple key/value
    // --------------------
    Key   k1 = "hello";
    Value v1{'w', 'o', 'r', 'l', 'd'};
    kv.put(k1, v1);
    dummy d;
    d.hi = 100;
    std::cout << "Inserted key=" << k1 << std::endl;

    // update metadata
    if (!kv.update_meta(k1, d))
    {
      std::cerr << "Something went wrong while updating metadata for k1!" << std::endl;
      exit(1);
    };

    // --------------------
    // 2. get copy
    // --------------------
    auto v1_copy = kv.get(k1);
    std::cout << "Fetched value: " << as::key(v1_copy) << std::endl;

    // --------------------
    // 3. metadata
    // --------------------
    auto meta1 = kv.meta(k1);
    if (meta1)
    {
      // you can print the entire struct out yourself (including extra granted you have operator overloaded << with your extra struct definition itself)
      std::cout << *meta1 << std::endl;
    }

    kv.print_meta(k1, [](std::ostream& os, const dummy& d) { os << "hi: " << d.hi; });

    // --------------------
    // 4. exists
    // --------------------
    std::cout << "Exists? " << kv.exists(k1) << std::endl;

    // --------------------
    // 5. get_view (zero-copy)
    // --------------------
    if (auto view = kv.get_view(k1))
    {
      Key s(view->data, view->size);
      std::cout << "Zero-copy view: " << s << std::endl;
      // view->txn_owner keeps txn alive until scope exit
    }

    // --------------------
    // 6. put from file
    // --------------------
    const Key file = "tmpfile.txt";
    std::ofstream(file) << "content-from-file";
    kv.put(file);
    std::cout << "Inserted filekey from: " << file << std::endl;

    // --------------------
    // 7. for_each
    // --------------------
    std::cout << "--- for_each(all) ---" << std::endl;
    kv.for_each("",
                [](const Key& k, const Value& v) {
                  std::cout << "  " << k << " -> " << std::string(v.begin(), v.end()) << std::endl;
                });

    // --------------------
    // 8. for_
    // --------------------
    std::cout << "--- for_ (shorthand for for_each) ---" << std::endl;
    kv.for_([](const Key& k, const Value& v)
            { std::cout << "  " << k << " -> " << std::string(v.begin(), v.end()) << std::endl; });

    // --------------------
    // 9. range_query (copy values)
    // --------------------
    std::cout << "--- range_query [f..z) ---" << std::endl;
    kv.range_query("f", "z",
                   [](const Key& k, const Value& v) {
                     std::cout << "  " << k << " -> " << std::string(v.begin(), v.end())
                               << std::endl;
                   });

    // --------------------
    // 10. range_query_view (zero-copy)
    // --------------------
    std::cout << "--- range_query_view [f..z) ---" << std::endl;
    kv.range_query_view("f", "z",
                        [](const KeyView& k, const ValueView& v)
                        {
                          Key key(k.data, k.size);
                          Key val(v.data, v.size);
                          std::cout << "  " << key << " -> " << val << std::endl;
                        });

    // --------------------
    // 11. batch (put + del together)
    // --------------------
    std::cout << "--- batch ops ---" << std::endl;
    kv.batch(
      [&](auto put, auto del)
      {
        put("batch1", {'x', 'y', 'z'});
        put("batch2", {'a', 'b', 'c'});
        del("hello"); // delete the original key
      });
    std::cout << "Batch operations done." << std::endl;

    // --------------------
    // 12. erase (remove key and metadata)
    // --------------------
    kv.erase("batch2");
    std::cout << "Erased key batch2" << std::endl;

    // --------------------
    // 13. ensure_map_size (grow mapsize manually)
    // --------------------
    kv.ensure_map_size(256ull * 1024 * 1024);
    std::cout << "Mapsized grown to >= 256MB" << std::endl;

    // --------------------
    // Final: list all keys
    // --------------------
    std::cout << "--- final DB content ---" << std::endl;
    kv.for_([](const Key& k, const Value& v)
            { std::cout << "  " << k << " -> " << as::key(v) << std::endl; });

    std::cout << "=== Done ===" << std::endl;
  }
  catch (const LMDBError& e)
  {
    std::cerr << "LMDB error: " << e.what() << " (code=" << e.code() << ")" << std::endl;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }

  return 0;
}
