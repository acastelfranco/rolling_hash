#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <endian.h>
#include <algorithm>
#include <cstdlib>

#include <BackupService.h>

static constexpr uint32_t CHUNKSIZ = 0xFF;

int main(int argc, const char **argv)
{
	BackupService::backup("starwars_a_new_hope.txt", "starwars_a_new_hope_modified.txt", CHUNKSIZ);
	BackupService::restore("starwars_a_new_hope.txt", "starwars_a_new_hope_modified.txt.deltas.bin", "new_starwars_story.txt");
}
