#pragma once
#include "app/context.hpp"

#include <cstdio>

namespace npps::app {

extern const char *const MAIN_SCHEMA_REVISION;
extern const char *const MAIN_SCHEMA[];

// Opens <data_directory>/main.sqlite3 (creating it with the NPPS4 schema)
// and the master databases. Master databases may be honky-encrypted game
// files; those are decrypted once into <data_directory>/db-cache.
void open_databases();
void close_databases();

// How the server opens game files; the Vita build points this at the
// asset pack aware fopen. Defaults to fopen.
extern FILE *(*open_game_file)(const char *path, const char *mode);

}  // namespace npps::app
