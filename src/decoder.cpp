#include "decoder.h"
#include "coder.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <numeric>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <random> 

namespace fs = std::filesystem;

/**
 * @brief Декодирует файл, закодированный с использованием словаря Шеннона.
 * @param encoded_filename Имя закодированного файла.
 * @return true, если декодирование успешно.
 */
bool decode_file(const std::string& encoded_filename) {
    std::ifstream encodedFile(encoded_filename, std::ios::binary);
    if (!encodedFile.is_open()) {
        std::cerr << "Не удалось открыть закодированный файл: " << encoded_filename << std::endl;
        return false;
    }

    // Проверяем размер файла
    encodedFile.seekg(0, std::ios::end);
    size_t file_size = encodedFile.tellg();
    encodedFile.seekg(0, std::ios::beg);

    if (file_size == 0) {
        std::cerr << "Закодированный файл пуст: " << encoded_filename << std::endl;
        encodedFile.close();
        return false;
    }

    unsigned char stored_dictionary_size;
    unsigned char stored_id;

    if (!encodedFile.read(reinterpret_cast<char*>(&stored_dictionary_size), sizeof(stored_dictionary_size)) ||
        !encodedFile.read(reinterpret_cast<char*>(&stored_id), sizeof(stored_id))) {
        std::cerr << "Ошибка при чтении служебной информации из закодированного файла." << std::endl;
        encodedFile.close();
        return false;
    }

    fs::path encoded_path = encoded_filename;
    std::string encoded_name_with_format = encoded_path.filename().string();
    size_t first_underscore = encoded_name_with_format.find('_');
    size_t second_underscore = encoded_name_with_format.find('_', first_underscore + 1);
    size_t dot_pos = encoded_name_with_format.find('.');

    if (first_underscore == std::string::npos || second_underscore == std::string::npos || dot_pos == std::string::npos || first_underscore >= second_underscore || second_underscore >= dot_pos) {
        std::cerr << "Неверный формат имени закодированного файла." << std::endl;
        encodedFile.close();
        return false;
    }

    std::string original_name_with_format = encoded_name_with_format.substr(second_underscore + 1);
    std::string original_name = original_name_with_format.substr(0, original_name_with_format.find('.'));
    std::replace(original_name.begin(), original_name.end(), ' ', '_'); // Заменяем пробелы на подчеркивания
    std::string dictionary_filename = "work/dictionary/dict_" + std::to_string(static_cast<int>(stored_id)) + "_" + original_name + ".bin";

    // Проверяем существование файла словаря
    if (!fs::exists(dictionary_filename)) {
        std::cerr << "Ошибка: файл словаря не существует: " << dictionary_filename << std::endl;
        encodedFile.close();
        return false;
    }

    unsigned char loaded_id;
    std::map<unsigned char, std::string> dictionary = load_dictionary(dictionary_filename, loaded_id);

    if (dictionary.empty()) {
        std::cerr << "Ошибка: словарь пуст или не удалось загрузить." << std::endl;
        encodedFile.close();
        return false;
    }

    if (stored_id != loaded_id) {
        std::cerr << "Ошибка: ID закодированного файла (" << static_cast<int>(stored_id)
                  << ") не совпадает с ID словаря (" << static_cast<int>(loaded_id) << ")." << std::endl;
        encodedFile.close();
        return false;
    }

    // Создаем обратный словарь для быстрого поиска байта по коду
    std::map<std::string, unsigned char> reverse_dictionary;
    for (const auto& pair : dictionary) {
        reverse_dictionary[pair.second] = pair.first;
    }

    std::string decoded_filename = "work/decoded/decoded_" + original_name_with_format;
    std::ofstream decodedFile(decoded_filename, std::ios::binary);
    if (!decodedFile.is_open()) {
        std::cerr << "Не удалось открыть файл для записи декодированных данных: " << decoded_filename << std::endl;
        encodedFile.close();
        return false;
    }

    unsigned char byte_read;

    // Буфер для накопления битов, которые будут преобразованы в символ
    std::string current_bit_buffer;

    // Счетчик записанных байтов в декодированный файл
    int bytes_written = 0;

    while (encodedFile.read(reinterpret_cast<char*>(&byte_read), sizeof(byte_read))) {
        // Проходим по каждому биту в байте (от старшего к младшему)
        for (int i = 7; i >= 0; --i) {
            // Проверяем, установлен ли текущий бит (i-й бит)
            if ((byte_read >> i) & 1) {
                // Если бит установлен, добавляем '1' в буфер
                current_bit_buffer += '1';
            } else {
                // Если бит не установлен, добавляем '0' в буфер
                current_bit_buffer += '0';
            }

            // Проверяем, есть ли текущая последовательность битов в обратном словаре
            if (reverse_dictionary.count(current_bit_buffer)) {
                // Если последовательность найдена, записываем соответствующий байт в декодированный файл
                decodedFile.write(reinterpret_cast<const char*>(&reverse_dictionary[current_bit_buffer]), sizeof(unsigned char));
                // Увеличиваем счетчик записанных байтов
                bytes_written++;
                // Очищаем буфер для следующей последовательности битов
                current_bit_buffer.clear();
            }
        }
    }

    if (bytes_written == 0) {
        std::cerr << "Ошибка: декодированные данные не были записаны." << std::endl;
        encodedFile.close();
        decodedFile.close();
        return false;
    }

    encodedFile.close();
    decodedFile.close();
    std::cout << "Файл успешно декодирован в: " << decoded_filename << std::endl;
    std::cout << "Записано байт: |" << bytes_written <<"|" <<std::endl;
    return true;
}

/**
 * @brief Отображает список закодированных файлов в указанной директории.
 * @param directory Директория для отображения файлов.
 */
void list_encoded_files(const std::string& directory) {
    std::cout << "Доступные закодированные файлы в '" << directory << "':" << std::endl;
    int i = 1;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename().string().find("encoded_") == 0) {
            std::cout << i++ << ". " << entry.path().filename().string() << std::endl;
        }
    }
    std::cout << "0. Выход" << std::endl;
}

