#ifndef CODER_H
#define CODER_H

#include <map>
#include <string>
#include <fstream>

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <numeric>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <random> 


std::map<unsigned char, int> calculate_frequency(const std::string& filename);
std::map<unsigned char, std::string> build_shannon_dictionary(const std::map<unsigned char, int>& frequency_map);
bool save_dictionary(const std::map<unsigned char, std::string>& dictionary, const std::string& filename, unsigned char random_byte);
std::map<unsigned char, std::string> load_dictionary(const std::string& filename, unsigned char& stored_random_byte);
bool encode_file(const std::string& input_filename);
void list_files(const std::string& directory);

#endif // CODER_H