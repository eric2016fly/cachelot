#include "unit_test.h"
#include <cachelot/stats.h>
#include <cachelot/cache.h>

namespace {

using namespace cachelot;

BOOST_AUTO_TEST_SUITE(test_cache_stats)

static auto calc_hash = fnv1a<cache::Cache::hash_type>::hasher();

cache::ItemPtr CreateItem(cache::Cache & c, const string k, const string v, cache::opaque_flags_type flags = 0, cache::seconds keepalive = cache::Item::infinite_TTL) {
    const auto key = slice(k.c_str(), k.length());
    const auto value = slice(v.c_str(), v.length());
    auto item = c.create_item(key, calc_hash(key), value.length(), flags, keepalive);
    item->assign_value(value);
    return item;
}

BOOST_AUTO_TEST_CASE(test_cache_commands_stats) {
    ResetStats();
    auto the_cache = cache::Cache::Create(4 * Megabyte, 4 * Kilobyte, 16, false);
    const auto non_existing = slice::from_literal("Non-existing key");
    // set
    {
        the_cache.do_set(CreateItem(the_cache, "Key1", "Valu1"));
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_set), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_new), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_existing), 0);
        the_cache.do_set(CreateItem(the_cache, "Key1", "Valu2"));
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_set), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_new), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,set_existing), 1);
    }
    // get
    {
        BOOST_CHECK(the_cache.do_get(non_existing, calc_hash(non_existing)) == nullptr);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_get), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_misses), 1);
        const auto the_key = slice::from_literal("Key1");
        BOOST_CHECK(the_cache.do_get(the_key, calc_hash(the_key)) != nullptr);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_get), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,get_misses), 1);
    }
    // add
    {
        BOOST_CHECK_EQUAL(the_cache.do_add(CreateItem(the_cache, "Add_Key1", "Value1")), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_add), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_not_stored), 0);
        BOOST_CHECK_EQUAL(the_cache.do_add(CreateItem(the_cache, "Add_Key1", "Value2")), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_add), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,add_not_stored), 1);
    }
    // replace
    {
        BOOST_CHECK_EQUAL(the_cache.do_replace(CreateItem(the_cache, "Replace_Key1", "Value1")), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_replace), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_not_stored), 1);
        the_cache.do_set(CreateItem(the_cache, "Replace_Key1", "Value1"));
        BOOST_CHECK_EQUAL(the_cache.do_replace(CreateItem(the_cache, "Replace_Key1", "Value2")), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_replace), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,replace_not_stored), 1);
    }
    // cas
    {
        bool found; bool stored;
        tie(found, stored) = the_cache.do_cas(CreateItem(the_cache, "CAS_Key1", "Value1"), 0);
        BOOST_CHECK_EQUAL(found, false);
        BOOST_CHECK_EQUAL(stored, false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 0);

        the_cache.do_set(CreateItem(the_cache, "CAS_Key1", "Value1"));
        auto key = slice::from_literal("CAS_Key1");
        auto item_timestamp = the_cache.do_get(key, calc_hash(key))->timestamp();
        tie(found, stored) = the_cache.do_cas(CreateItem(the_cache, "CAS_Key1", "Value2"), item_timestamp);
        BOOST_CHECK_EQUAL(found, true);
        BOOST_CHECK_EQUAL(stored, true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 0);

        tie(found, stored) = the_cache.do_cas(CreateItem(the_cache, "CAS_Key1", "Value3"), item_timestamp);
        BOOST_CHECK_EQUAL(found, true);
        BOOST_CHECK_EQUAL(stored, false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_cas), 3);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_misses), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cas_badval), 1);
    }
    // delete
    {
        BOOST_CHECK_EQUAL(the_cache.do_delete(non_existing, calc_hash(non_existing)), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_delete), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_misses), 1);
        the_cache.do_set(CreateItem(the_cache, "Delete_Key1", "Value1"));
        const auto the_key = slice::from_literal("Delete_Key1");
        BOOST_CHECK_EQUAL(the_cache.do_delete(the_key, calc_hash(the_key)), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_delete), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,delete_misses), 1);
    }
    // touch
    {
        BOOST_CHECK_EQUAL(the_cache.do_touch(non_existing, calc_hash(non_existing), cache::Item::infinite_TTL), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_touch), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_misses), 1);
        the_cache.do_set(CreateItem(the_cache, "Touch_Key1", "Value1"));
        const auto the_key = slice::from_literal("Touch_Key1");
        BOOST_CHECK_EQUAL(the_cache.do_touch(the_key, calc_hash(the_key), cache::Item::infinite_TTL), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_touch), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,touch_misses), 1);
    }
    // incr / decr
    {
        the_cache.do_incr(non_existing, calc_hash(non_existing), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_incr), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_misses), 1);
        the_cache.do_decr(non_existing, calc_hash(non_existing), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_decr), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_hits), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_misses), 1);
        the_cache.do_set(CreateItem(the_cache, "Arithmetic_Key1", "0"));
        const auto the_key = slice::from_literal("Arithmetic_Key1");
        the_cache.do_incr(the_key, calc_hash(the_key), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_incr), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,incr_misses), 1);
        the_cache.do_decr(the_key, calc_hash(the_key), 1ull);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_decr), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_hits), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,decr_misses), 1);
    }
    // append
    {
        BOOST_CHECK_EQUAL(the_cache.do_append(CreateItem(the_cache, "Append_Key1", "Value1")), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_append), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_misses), 1);
        the_cache.do_set(CreateItem(the_cache, "Append_Key1", "Value1"));
        BOOST_CHECK_EQUAL(the_cache.do_append(CreateItem(the_cache, "Append_Key1", "Value2")), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_append), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,append_misses), 1);
    }
    // prepend
    {
        BOOST_CHECK_EQUAL(the_cache.do_prepend(CreateItem(the_cache, "Prepend_Key1", "Value1")), false);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_prepend), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_stored), 0);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_misses), 1);
        the_cache.do_set(CreateItem(the_cache, "Prepend_Key1", "Value1"));
        BOOST_CHECK_EQUAL(the_cache.do_prepend(CreateItem(the_cache, "Prepend_Key1", "Value2")), true);
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_prepend), 2);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_stored), 1);
        BOOST_CHECK_EQUAL(STAT_GET(cache,prepend_misses), 1);
    }
    // flush_all
    {
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_flush), 0);
        the_cache.do_flush_all();
        BOOST_CHECK_EQUAL(STAT_GET(cache,cmd_flush), 1);
    }
}

BOOST_AUTO_TEST_CASE(test_cache_size_stats) {
    auto the_cache = cache::Cache::Create(4 * Megabyte, 4 * Kilobyte, 16, false);
    ResetStats();
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 16);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 0);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
    std::vector<string> keys;
    for (unsigned i = 0; i < 16; ++i) {
        auto k = random_string(10, 15);
        keys.push_back(k);
        const auto item = CreateItem(the_cache, k, random_string(1, 30));
        BOOST_CHECK_EQUAL(the_cache.do_add(item), true);
    }
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 32);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 16);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
    for (auto k : keys) {
        auto key = slice(k.c_str(), k.length());
        BOOST_CHECK_EQUAL(the_cache.do_delete(key, calc_hash(key)), true);
    }
    the_cache.publish_stats();
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_capacity), 32);
    BOOST_CHECK_EQUAL(STAT_GET(cache,curr_items), 0);
    BOOST_CHECK_EQUAL(STAT_GET(cache,hash_is_expanding), false);
}


BOOST_AUTO_TEST_SUITE_END()

} //anonymous namespace

