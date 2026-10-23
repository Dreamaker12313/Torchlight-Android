// The port's book of unit records: names are matched without case.
#include "core/units.h"
#include "test_support.h"

TL_TEST(a_unit_is_found_by_its_name_in_any_case) {
    tl::core::UnitBook book;
    tl::core::UnitStats zealot;
    zealot.display_name = "Zealot";
    tl::core::UnitStats skeleton;
    skeleton.display_name = "Skeleton Archer";
    book.add("Zealot", zealot);
    book.add("SKELETON_ARCHER_LONG_NAME", skeleton);
    TL_CHECK(book.find("zealot") != nullptr);
    TL_CHECK(book.find("ZEALOT") != nullptr);
    TL_CHECK(book.find("Skeleton_Archer_Long_Name") != nullptr);
    TL_CHECK_EQ(book.find("skeleton_archer_long_name")->display_name,
                std::string("Skeleton Archer"));
    TL_CHECK(book.find("zealo") == nullptr);
    TL_CHECK(book.find("zealots") == nullptr);
    TL_CHECK(book.find("") == nullptr);
    // A second add under another case replaces the record, as one name.
    tl::core::UnitStats again;
    again.display_name = "Zealot II";
    book.add("zEaLoT", again);
    TL_CHECK_EQ(book.size(), size_t{2});
    TL_CHECK_EQ(book.find("Zealot")->display_name, std::string("Zealot II"));
}
