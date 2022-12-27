#include <boost/container/flat_map.hpp>

#include "../implementation/flat_map"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <vector>
#include <random>


enum map_impl_kind
{
    boost_flat_map,
    std_map,
    split_map,

    num_map_impl_kinds
};

template <typename T, typename U>
using split_map_t = std::flat_map<T, U>;

template <typename KeyType, typename ValueType, map_impl_kind MapImpl>
struct map_impl
{
    using type = std::map<KeyType, ValueType>;
};

template <typename KeyType, typename ValueType>
struct map_impl<KeyType, ValueType, boost_flat_map>
{
    using type = boost::container::flat_map<KeyType, ValueType>;
};

template <typename KeyType, typename ValueType>
struct map_impl<KeyType, ValueType, split_map>
{
    using type = split_map_t<KeyType, ValueType>;
};

template <typename KeyType, typename ValueType, map_impl_kind MapImpl>
using map_impl_t = typename map_impl<KeyType, ValueType, MapImpl>::type;

template <typename KeyType>
KeyType make_key(int x)
{ return x; }

template <>
std::string make_key(int x)
{ return std::to_string(x); }

template <typename ValueType>
ValueType make_value()
{ return ValueType(); }

template <typename T>
auto value_of(T const & x)
{ return x; }

template <typename T, typename U>
auto value_of(std::pair<T, U> const & x)
{ return x.second; }

struct output_files_t
{
    std::ofstream ofs[num_map_impl_kinds];
};

double single_elapsed_value(std::vector<double> & times)
{
    // Take the mean after throwing out the smallest and largest values.
    std::sort(times.begin(), times.end());
    return std::accumulate(times.begin() + 1, times.end() - 1, 0.0) / (times.size() - 2);
}

struct StopWatch
{
    StopWatch() : 
        start(std::chrono::high_resolution_clock::now())
    {
    }
    double observe() const
    {
        return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count() * 1000;
    }
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;
};

template <typename KeyType, typename ValueType, map_impl_kind MapImpl, int Iterations>
void test_map_type(std::string kind_name, std::vector<int> const & v, output_files_t & output_files)
{
    using map_t = map_impl_t<KeyType, ValueType, MapImpl>;

    std::vector<map_t> maps(Iterations);

    int const other_map_factor = 64;
    std::vector<map_t> other_maps_were_not_measuring(other_map_factor * Iterations);

    output_files.ofs[MapImpl] << "    {'size': " << v.size() << ", ";

    kind_name += ':';
    kind_name += std::string(40 - kind_name.size(), ' ');

    {
        std::vector<double> times;
        for (int i = 0, size = (int)maps.size(); i < size; ++i) {
            map_t & map = maps[i];
            double time = 0.0;
            for (auto e : v)
            {
                // allocate a bunch of nodes of the same size to fragment
                // memory.
                for (int j = 0; j < other_map_factor; ++j) {
                    map_t & other_map =
                        other_maps_were_not_measuring[other_map_factor * i + j];
                    auto const key = make_key<KeyType>(other_map_factor * e + j);
                    other_map[key] = make_value<ValueType>();
                }
                auto const key = make_key<KeyType>(e);
                StopWatch stopWatch;
                map[key] = make_value<ValueType>();
                time += stopWatch.observe();
            }
            times.push_back(time);
        }
        auto const elapsed = single_elapsed_value(times);
        output_files.ofs[MapImpl] << "'insert': " << elapsed << ",";
        std::cout << "  " << kind_name << elapsed << " ms insert\n";
    }

    {
        std::vector<double> times;
        int copy_count = 0; // To ensure the optimizer does not remove the loops below altogether, do some work.
        for (auto const & map : maps) {
            std::vector<ValueType> values(map.size());
                StopWatch stopWatch;
            std::transform(
                begin(map), end(map), begin(values),
                [](auto const & elem){ return value_of(elem); }
            );
            auto stop = std::chrono::high_resolution_clock::now();
            times.push_back(stopWatch.observe());
            for (auto x : values) {
                ++copy_count;
            }
        }
        auto const elapsed = single_elapsed_value(times);
        output_files.ofs[MapImpl] << "'iterate': " << elapsed << ",";
        std::cout << "  " << kind_name << elapsed << " ms iterate\n";
        if (copy_count == 2)
            std::cout << "  SURPRISE! copy_count=" << copy_count << "\n";
    }

    {
        std::vector<double> times;
        int key_count = 0; // To ensure the optimizer does not remove the loops below altogether, do some work.
        for (auto & map : maps) {
            auto const end_ = end(map);
            double time = 0.0;
            for (auto e : v) {
                auto const key = make_key<KeyType>(e);
                StopWatch stopWatch;
                auto const it = map.find(key);
                if (it != end_)
                    ++key_count;
                time += stopWatch.observe();
            }
            times.push_back(time);
        }
        auto const elapsed = single_elapsed_value(times);
        output_files.ofs[MapImpl] << "'find': " << elapsed << ",";
        std::cout << "  " << kind_name << elapsed << " ms find\n";
        if (key_count == 2)
            std::cout << "  SURPRISE! key_count=" << key_count << "\n";
    }

    output_files.ofs[MapImpl] << "},\n";
}

template <typename KeyType, typename ValueType>
void test(std::size_t size, output_files_t & output_files)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist;

    auto rand = [&]() {
        return dist(gen);
    };

    std::vector<int> v(size);
    std::generate(v.begin(), v.end(), rand);

    int const iterations = 7;

    test_map_type<KeyType, ValueType, boost_flat_map, iterations>("boost flat_map", v, output_files);
    test_map_type<KeyType, ValueType, std_map, iterations>("std::map", v, output_files);
//    test_map_type<KeyType, ValueType, split_map, iterations>("split_map", v, output_files);

    std::cout << std::endl;
}

template <typename KeyType, typename ValueType, map_impl_kind MapImpl>
auto makeMap(std::vector<int> const & v) -> map_impl_t<KeyType, ValueType, MapImpl>
{
    using map_t = map_impl_t<KeyType, ValueType, MapImpl>;
    map_t map;

    for(auto x : v)
    {
        auto const key = make_key<KeyType>(x);
        map[key] = make_value<ValueType>();    
    }
    return map;
}

template <typename KeyType, typename ValueType, map_impl_kind MapImpl, int Iterations, typename _Pred>
void test2_map_type(std::string kind_name, std::vector<int> const & v1,std::vector<int> const & v2, output_files_t & /*output_files*/, _Pred pred)
{
    using map_t = map_impl_t<KeyType, ValueType, MapImpl>;

    std::vector<int> vUnion, vIntersection;

    std::set_union(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(vUnion));
    //std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(vIntersection));

    map_t map1 = makeMap<KeyType, ValueType, MapImpl>(v1);
    map_t map2 = makeMap<KeyType, ValueType, MapImpl>(v2);
    map_t mapUnion = makeMap<KeyType, ValueType, MapImpl>(vUnion);

    kind_name += ':';
    kind_name += std::string(40 - kind_name.size(), ' ');

    {
        std::vector<double> times;
        for (int i = 0; i < Iterations; ++i) {
            map_t map12 = map1;
            map_t map2_copy = map2;
            StopWatch stopWatch;
            pred(map12, map2_copy);
            times.push_back(stopWatch.observe());
            if(mapUnion != map12)
            {
                // Prevent the optimizer form getting too aggressive.
                abort();
            }
        }
        auto const elapsed = single_elapsed_value(times);
        //output_files.ofs[MapImpl] << "'insert': " << elapsed << ",";
        std::cout << "  " << kind_name << elapsed << " ms merge\n";
    }

    //output_files.ofs[MapImpl] << "},\n";
}

template <typename KeyType, typename ValueType>
void test2(std::size_t size, output_files_t & output_files)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, size*2);

    auto rand = [&]() {
        return dist(gen);
    };

    std::vector<int> v1(size), v2(size);
    std::generate(v1.begin(), v1.end(), rand);
    std::generate(v2.begin(), v2.end(), rand);
    std::sort(v1.begin(), v1.end());
    std::sort(v2.begin(), v2.end());
    v1.erase(std::unique(v1.begin(), v1.end()), v1.end());
    v2.erase(std::unique(v2.begin(), v2.end()), v2.end());

    int const iterations = 7;

    test2_map_type<KeyType, ValueType, std_map, iterations>("std::map", v1, v2, output_files,
    [](auto &map1, auto &map2)
    {
        map1.merge(map2);
    });
    test2_map_type<KeyType, ValueType, boost_flat_map, iterations>("boost flat_map", v1, v2, output_files,
    [](auto &map1, auto &map2)
    {
        map1.merge(map2);
    });
    test2_map_type<KeyType, ValueType, split_map, iterations>("flat_map insert sort", v1, v2, output_files,
    [](auto &map1, auto &map2)
    {
        map1.insert(map2.begin(), map2.end());
    });
    test2_map_type<KeyType, ValueType, split_map, iterations>("flat_map", v1, v2, output_files,
    [](auto &map1, auto &map2)
    {
        map1.merge(map2);
    });

//    std::cout << std::endl;
}

#define TEST(key_t, value_t, size)                              \
    std::cout << "<" << #key_t << ", " << #value_t << ">, "     \
              << (size) << " elements:\n";                      \
    /*test<key_t, value_t>((size), output_files);*/ \
    test2<key_t, value_t>((size), output_files)

int main()
{
    output_files_t output_files;
    output_files.ofs[boost_flat_map].open("boost_flat_map.py");
    output_files.ofs[std_map].open("std_map.py");
    output_files.ofs[split_map].open("split_map.py");

    for (auto & of : output_files.ofs) {
        of << "int_timings = [\n";
    }

#if 1
    TEST(int, int, 8u);
    TEST(int, int, 8u << 1);
    TEST(int, int, 8u << 2);
    TEST(int, int, 8u << 3);
    TEST(int, int, 8u << 4);
    TEST(int, int, 8u << 5);
    TEST(int, int, 8u << 6);
    TEST(int, int, 8u << 7);
    TEST(int, int, 8u << 8);
    TEST(int, int, 8u << 9);
    TEST(int, int, 8u << 10);
    TEST(int, int, 8u << 11);
    TEST(int, int, 8u << 12);
    TEST(int, int, 8u << 13);
    TEST(int, int, 8u << 14);
    TEST(int, int, 8u << 15);
#if 0
#endif
#else
    TEST(int, int, 10u);
    TEST(int, int, 100u);
    TEST(int, int, 1000u);
    TEST(int, int, 10000u);
    TEST(int, int, 100000u);
#endif

    for (auto & of : output_files.ofs) {
        of << "]\n\n"
           << "string_timings = [\n";
    }

#if 1
    TEST(std::string, std::string, 8u);
    TEST(std::string, std::string, 8u << 1);
    TEST(std::string, std::string, 8u << 2);
    TEST(std::string, std::string, 8u << 3);
    TEST(std::string, std::string, 8u << 4);
    TEST(std::string, std::string, 8u << 5);
    TEST(std::string, std::string, 8u << 6);
    TEST(std::string, std::string, 8u << 7);
    TEST(std::string, std::string, 8u << 8);
    TEST(std::string, std::string, 8u << 9);
    TEST(std::string, std::string, 8u << 10);
    TEST(std::string, std::string, 8u << 11);
    TEST(std::string, std::string, 8u << 12);
    TEST(std::string, std::string, 8u << 13);
    TEST(std::string, std::string, 8u << 14);
    TEST(std::string, std::string, 8u << 15);
#if 0
#endif
#else
    TEST(std::string, std::string, 10u);
    TEST(std::string, std::string, 100u);
    TEST(std::string, std::string, 1000u);
    TEST(std::string, std::string, 10000u);
    TEST(std::string, std::string, 100000u);
#endif

    for (auto & of : output_files.ofs) {
        of << "]\n";
    }
}
