#ifndef DECODER_H
#define DECODER_H

#include <string>

bool decode_file(const std::string& encoded_filename);
void list_encoded_files(const std::string& directory);

#endif // DECODER_H