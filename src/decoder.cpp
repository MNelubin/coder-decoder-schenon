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
 *
 * Эта функция открывает закодированный файл, считывает заголовок (исходный размер, размер словаря, ID),
 * загружает соответствующий словарь, выполняет проверки и декодирует данные, записывая их
 * в новый файл.
 *
 * @param encoded_filename Имя закодированного файла.
 * @return true, если декодирование успешно, false в противном случае.
 */
bool decode_file(const std::string& encoded_filename) {
    std::ifstream encodedFile(encoded_filename, std::ios::binary);
    if (!encodedFile.is_open()) {
        std::cerr << "Не удалось открыть закодированный файл: " << encoded_filename << std::endl;
        return false;
    }

    // Проверяем, достаточно ли данных для чтения заголовка
    encodedFile.seekg(0, std::ios::end);
    size_t file_size_check = encodedFile.tellg();
    encodedFile.seekg(0, std::ios::beg);

    // Минимальный заголовок: original_size (8) + dictionary_map_size (1) + id (1) = 10 байт
    if (file_size_check < sizeof(uint64_t) + sizeof(unsigned char) + sizeof(unsigned char)) {
        std::cerr << "Ошибка: Закодированный файл слишком мал для чтения заголовка." << std::endl;
        encodedFile.close();
        return false;
    }

    uint64_t original_file_size;
    unsigned char stored_dictionary_map_size; // Размер карты словаря
    unsigned char stored_id;

    // 1. Читаем исходный размер файла
    if (!encodedFile.read(reinterpret_cast<char*>(&original_file_size), sizeof(original_file_size))) {
        std::cerr << "Ошибка при чтении original_file_size из заголовка." << std::endl; return false;
    }
    // 2. Читаем размер словаря (количество записей)
    if (!encodedFile.read(reinterpret_cast<char*>(&stored_dictionary_map_size), sizeof(stored_dictionary_map_size))) {
         std::cerr << "Ошибка при чтении stored_dictionary_map_size из заголовка." << std::endl; return false;
    }
    // 3. Читаем ID
    if (!encodedFile.read(reinterpret_cast<char*>(&stored_id), sizeof(stored_id))) {
        std::cerr << "Ошибка при чтении stored_id из заголовка." << std::endl; return false;
    }

    // Извлекаем имя исходного файла из имени закодированного файла
    fs::path encoded_path = encoded_filename;
    std::string encoded_name_with_format = encoded_path.filename().string();
    size_t first_underscore = encoded_name_with_format.find('_');
    size_t second_underscore = encoded_name_with_format.find('_', first_underscore + 1);

    if (first_underscore == std::string::npos || second_underscore == std::string::npos) { 
        std::cerr << "Неверный формат имени закодированного файла для извлечения original_name." << std::endl;
        encodedFile.close();
        return false;
    }
    // Имя файла без префикса "encoded_" и ID, но с расширением
    std::string original_name_with_format = encoded_name_with_format.substr(second_underscore + 1);

    // Имя файла без расширения для поиска словаря
    std::string base_original_name_for_dict = original_name_with_format.substr(0, original_name_with_format.rfind('.'));


    // Формируем имя файла словаря на основе ID и имени исходного файла
    std::string dictionary_filename = "work/dictionary/dict_" + std::to_string(static_cast<int>(stored_id)) + "_" + base_original_name_for_dict + ".bin";

    // Проверяем существование файла словаря
    if (!fs::exists(dictionary_filename)) {
        std::cerr << "Ошибка: файл словаря не существует: " << dictionary_filename << std::endl;
        encodedFile.close();
        return false;
    }

    // Загружаем словарь
    unsigned char loaded_id_from_dict;
    std::map<unsigned char, std::string> dictionary = load_dictionary(dictionary_filename, loaded_id_from_dict);

    // Проверяем соответствие ID
    if (stored_id != loaded_id_from_dict) {
        std::cerr << "Ошибка: ID закодированного файла (" << static_cast<int>(stored_id)
                  << ") не совпадает с ID словаря (" << static_cast<int>(loaded_id_from_dict) << ")." << std::endl;
        encodedFile.close();
        return false;
    }

    // Проверка, соответствует ли stored_dictionary_map_size реальному размеру загруженного словаря
    if (dictionary.size() != static_cast<size_t>(stored_dictionary_map_size)) {
        std::cerr << "Предупреждение: размер словаря в заголовке (" << static_cast<int>(stored_dictionary_map_size)
                  << ") не совпадает с реальным размером загруженного словаря (" << dictionary.size() << ")." << std::endl;
    }

    // Если исходный файл был пуст (original_file_size == 0), создаем пустой декодированный файл и завершаем
    if (original_file_size == 0) {
        std::string decoded_filename = "work/decoded/decoded_" + original_name_with_format;
        std::ofstream decodedFile(decoded_filename, std::ios::binary | std::ios::trunc); // Создать/очистить
        if (!decodedFile.is_open()) {
             std::cerr << "Не удалось создать пустой декодированный файл: " << decoded_filename << std::endl;
             encodedFile.close();
             return false;
        }
        decodedFile.close();
        std::cout << "Файл (пустой) успешно декодирован в: " << decoded_filename << std::endl;
        encodedFile.close();
        return true;
    }

    // Если словарь пуст, а original_file_size > 0, это ошибка.
    if (dictionary.empty() && original_file_size > 0) {
        std::cerr << "Ошибка: словарь пуст, но ожидается непустой декодированный файл." << std::endl;
        encodedFile.close();
        return false;
    }


    // Создаем обратный словарь для быстрого поиска байта по коду
    std::map<std::string, unsigned char> reverse_dictionary;
    for (const auto& pair : dictionary) {
        if (pair.second.empty()) { // Коды в словаре не должны быть пустыми
             std::cerr << "Критическая ошибка: пустой код в словаре для символа " << static_cast<int>(pair.first) << std::endl;
             encodedFile.close();
             return false;
        }
        reverse_dictionary[pair.second] = pair.first;
    }

    // Формируем имя выходного декодированного файла
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
    uint64_t bytes_written = 0; // Используем uint64_t для соответствия original_file_size

    // Читаем закодированный файл побайтово
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

                // Если достигнут исходный размер файла, прекращаем декодирование
                if (bytes_written == original_file_size) {
                    // Дополнительные биты в конце файла игнорируются
                    goto decoding_complete; // Выходим из вложенных циклов
                }
            }
        }
    }
// да я использовал goto, но это не очень хорошо, но я не знаю как сделать лучше
decoding_complete:; // Метка для перехода

    // Проверяем, соответствует ли количество записанных байтов исходному размеру файла
    if (bytes_written != original_file_size) {
        std::cerr << "Ошибка: Количество декодированных байтов (" << bytes_written
                  << ") не соответствует исходному размеру файла (" << original_file_size << ")." << std::endl;
        
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

