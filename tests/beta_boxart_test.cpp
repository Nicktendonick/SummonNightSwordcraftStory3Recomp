#include "beta_boxart.h"
#include <string_view>
#include <cstdio>
int main() {
    using namespace swordcraft3;
    if(next_beta_boxart("")!="clean" || next_beta_boxart("clean")!="archer" ||
       next_beta_boxart("original")!="clean" || next_beta_boxart("broken")!="clean") return 1;
    auto selected=next_beta_boxart("");
    for(int i=0;i<100;i++) {
        if(selected!=beta_artwork[i%beta_artwork.size()].token) return 2;
        if(std::string_view(beta_boxart_path(selected))!=beta_artwork[i%beta_artwork.size()].path) return 4;
        selected=next_beta_boxart(selected);
    }
    if(std::string_view(beta_boxart_path("original"))!="assets/beta/boxart-original.png" ||
       std::string_view(beta_boxart_path("clean"))!="assets/beta/boxart-clean.png" ||
       std::string_view(beta_boxart_path("../../file"))!="assets/beta/boxart-clean.png") return 3;
    std::puts("PASS: all 11 artworks cycle, initial/corrupt state, fixed asset paths");
}
