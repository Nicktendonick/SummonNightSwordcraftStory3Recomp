#include "guard_experiment.h"
#include "ram_write_override.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#define CHECK(x) do { if (!(x)) throw std::runtime_error("Failed: " #x); } while (0)
int main(int argc, char** argv) {
    std::array<std::uint8_t, 0x8000> ram{};
    auto w32 = [&](unsigned at, std::uint32_t value) {
        for (unsigned n=0;n<4;++n) ram[at+n] = std::uint8_t(value >> (n*8));
    };
    w32(0x6ac0,0x03000000); w32(0x6ab4,4); ram[0xc]=2;
    swordcraft3::GuardExperiment guard{true,ram.data(),ram.size()};
    std::uint32_t value=0;
    CHECK(guard.manual());
    for (unsigned keys=0;keys<1024;++keys) {
        ram[0x594c]=keys; ram[0x594d]=keys>>8;
        CHECK(guard.read(0x080272e2,0x0300594c,2,keys,0,&value));
        CHECK(value == ((keys & ~4u) | ((keys & 4u) ? 2u : 0u)));
        CHECK((value & ~6u) == (keys & ~6u));
        CHECK(guard.read(0x08029966,0x03005920,2,keys,0,&value));
        CHECK(value == (keys & ~4u));
        CHECK(!guard.read(0x0802999a,0x03005920,2,keys,0,&value));
    }
    ram[0x594c]=4; ram[0x594d]=0;
    for (unsigned slot=0;slot<=5;++slot) {
        ram[0xa26]=slot;
        const auto before=ram;
        CHECK(guard.read(0x08042698,0x03000a26,1,slot,0x030008c0,&value) && value==0);
        CHECK(guard.read(0x080426dc,0x03000a26,1,slot,0x030008c0,&value) && value==0);
        CHECK(guard.write(0x08042862,0x03000a26,1,0,0x030008c0,&value) && value==slot);
        CHECK(guard.read(0x08042866,0x03006ac0,4,0x03000000,0x030008c0,&value) && value==0);
        CHECK(ram==before);
    }
    CHECK(!guard.read(0x08042698,0x03000db6,1,2,0x03000c50,&value)); // enemy
    CHECK(!guard.read(0x08042698,0x03000a26,2,2,0x030008c0,&value)); // width
    CHECK(!guard.read(0x08042696,0x03000a26,1,2,0x030008c0,&value)); // site
    CHECK(!guard.write(0x08042862,0x03000a26,1,1,0x030008c0,&value));
    CHECK(!guard.write(0x08042862,0x03000a27,1,0,0x030008c0,&value));
    ram[0x594c]=0;
    CHECK(!guard.write(0x08042862,0x03000a26,1,0,0x030008c0,&value));
    CHECK(!guard.read(0x080426dc,0x03000a26,1,3,0x030008c0,&value));
    ram[0x594c]=4;
    for (unsigned phase=0;phase<16;++phase) {
        w32(0x6ab4,phase); CHECK(guard.manual()==(phase==4));
    }
    w32(0x6ab4,4);
    for (unsigned mode=0;mode<12;++mode) { ram[0xc]=mode; CHECK(guard.manual()==(mode==2)); }
    ram[0xc]=2;
    ram[0xf]=1; CHECK(!guard.manual()); ram[0xf]=0;
    ram[0xd]=3; CHECK(!guard.manual()); ram[0xd]=0;
    for (unsigned mode=1;mode<=2;++mode) { ram[0x12]=mode; CHECK(!guard.manual()); }
    ram[0x12]=0;
    w32(0x6ac0,0x03000004); CHECK(!guard.manual()); w32(0x6ac0,0x03000000);
    guard.supported=false; CHECK(!guard.manual()); guard.supported=true;
    guard.size=0x6ac3; CHECK(!guard.manual()); guard.size=ram.size();
    guard.ram=nullptr; CHECK(!guard.manual()); guard.ram=ram.data();
    CHECK(!swordcraft3::guard_rom_supported(nullptr,0x1000000));
    CHECK(!swordcraft3::guard_rom_supported(ram.data(),ram.size()));

    auto replace = +[](std::uint32_t, std::uint32_t, unsigned, std::uint32_t, std::uint32_t* out) {
        *out=0xabcdef12; return true;
    };
    auto reject = +[](std::uint32_t, std::uint32_t, unsigned, std::uint32_t, std::uint32_t*) { return false; };
    using gbarecomp::mapped_ram_write;
    CHECK(mapped_ram_write(nullptr,0,0x03000000,1,7)==7);
    CHECK(mapped_ram_write(reject,0,0x03000000,1,7)==7);
    CHECK(mapped_ram_write(replace,0,0x03000000,1,7)==0x12);
    CHECK(mapped_ram_write(replace,0,0x0203fffe,2,7)==0xef12);
    CHECK(mapped_ram_write(replace,0,0x03007ffc,4,7)==0xabcdef12);
    for (unsigned addr : {0x00000000u,0x04000000u,0x08000000u,0x0e000000u,0x03008000u,0x02040000u,0xffffffffu})
        CHECK(mapped_ram_write(replace,0,addr,1,7)==7);
    CHECK(mapped_ram_write(replace,0,0x03007ffe,4,7)==7);
    CHECK(mapped_ram_write(replace,0,0x03000000,3,7)==7);
    for (int n=1;n<argc;++n) {
        std::ifstream input(argv[n],std::ios::binary);
        CHECK(input.good());
        std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(input)),{});
        CHECK(swordcraft3::guard_rom_supported(rom.data(),rom.size()));
        rom[0x42862]^=1;
        CHECK(!swordcraft3::guard_rom_supported(rom.data(),rom.size()));
    }
    std::cout << "PASS: 1024 input combinations, all six slots, lifecycle/actor/site/ROM guards, RAM-only value overrides\n";
}
