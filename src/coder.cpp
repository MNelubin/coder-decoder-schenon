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
 * @brief Вычисляет частоту байтов в файле.
 * @param filename Имя файла для анализа.
 * @return Карта частот байтов.
 */
std::map<unsigned char, int> calculate_frequency(const std::string& filename) {
    std::map<unsigned char, int> frequency_map;
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл: " << filename << std::endl;
        return frequency_map; // Возвращаем пустую карту в случае ошибки
    }

    unsigned char byte;
    while (file.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        frequency_map[byte]++;
    }

    file.close();
    return frequency_map;
}

/**
 * @brief Сравнивает пары по убыванию частоты.
 * @param a Первая пара.
 * @param b Вторая пара.
 * @return true, если частота первой пары больше.
 */
bool compareFrequency(const std::pair<unsigned char, int>& a, const std::pair<unsigned char, int>& b) {
    return a.second > b.second;
}

/**
 * @brief Строит словарь Шеннона на основе частот байтов.
 * @param frequency_map Карта частот байтов.
 * @return Словарь Шеннона.
 */
std::map<unsigned char, std::string> build_shannon_dictionary(const std::map<unsigned char, int>& frequency_map) {
    std::map<unsigned char, std::string> dictionary;
    std::vector<std::pair<unsigned char, int>> sorted_frequencies(frequency_map.begin(), frequency_map.end());

    // Сортируем частоты по убыванию
    std::sort(sorted_frequencies.begin(), sorted_frequencies.end(), compareFrequency);

    size_t n = sorted_frequencies.size();
    std::vector<double> probabilities(n);
    
    //подсчет количесвта встреч байтов
    double total_frequency = std::accumulate(frequency_map.begin(), frequency_map.end(), 0.0,
                                             [](double sum, const std::pair<unsigned char, int>& p) {
                                                 return sum + p.second;
                                             });

    // Вычисляем вероятности
    for (size_t i = 0; i < n; ++i) {
        probabilities[i] = sorted_frequencies[i].second / total_frequency;
    }

    std::vector<double> cumulative_probabilities(n, 0.0);
    cumulative_probabilities[0] = 0.0;
    for (size_t i = 1; i < n; ++i) {
        cumulative_probabilities[i] = cumulative_probabilities[i - 1] + probabilities[i - 1];
    }

    // Генерируем коды Шеннона
    for (size_t i = 0; i < n; ++i) {
        std::string code = ""; // Инициализируем пустую строку для кода
        double p = cumulative_probabilities[i]; // Начальное значение кумулятивной вероятности
        int code_length = std::ceil(-std::log2(probabilities[i])); // Вычисляем длину кода

        // Генерируем код Шеннона
        for (int j = 0; j < code_length; ++j) {
            p *= 2.0; // Удваиваем значение p
            if (p >= 1.0) {
                code += '1'; // Добавляем '1' к коду
                p -= 1.0; // Вычитаем 1.0 из p
            } else {
                code += '0'; // Добавляем '0' к коду
            }
        }

        // Сохраняем сгенерированный код в словаре
        dictionary[sorted_frequencies[i].first] = code;
    }

    return dictionary;
}

/**
 * @brief Сохраняет словарь Шеннона в файл.
 * @param dictionary Словарь Шеннона.
 * @param filename Имя файла для сохранения.
 * @param random_byte Случайный байт для идентификации словаря.
 * @return true, если сохранение успешно.
 */
bool save_dictionary(const std::map<unsigned char, std::string>& dictionary, const std::string& filename, unsigned char random_byte) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл для записи словаря: " << filename << std::endl;
        return false;
    }

    // Используем uint16_t для хранения длины словаря (2 байта)
    uint16_t dictionary_size = static_cast<uint16_t>(dictionary.size());
    file.write(reinterpret_cast<const char*>(&dictionary_size), sizeof(dictionary_size));
    file.write(reinterpret_cast<const char*>(&random_byte), sizeof(random_byte));

    for (const auto& pair : dictionary) {
        unsigned char byte = pair.first;
        std::string code = pair.second;
        uint16_t code_length = static_cast<uint16_t>(code.length()); // Используем uint16_t для длины кода

        file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
        file.write(reinterpret_cast<const char*>(&code_length), sizeof(code_length));
        file.write(code.data(), code_length);
    }

    file.close();
    std::cout << "Словарь успешно сохранен в файл: " << filename << std::endl;
    return true;
}

/**
 * @brief Загружает словарь Шеннона из файла.
 * @param filename Имя файла для загрузки.
 * @param stored_random_byte Случайный байт, сохраненный в файле.
 * @return Словарь Шеннона.
 */
std::map<unsigned char, std::string> load_dictionary(const std::string& filename, unsigned char& stored_random_byte) {
    std::map<unsigned char, std::string> dictionary;
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл для чтения словаря: " << filename << std::endl;
        return dictionary;
    }

    // Читаем длину словаря как uint16_t (2 байта)
    uint16_t dictionary_size;
    file.read(reinterpret_cast<char*>(&dictionary_size), sizeof(dictionary_size));
    file.read(reinterpret_cast<char*>(&stored_random_byte), sizeof(stored_random_byte));

    for (int i = 0; i < dictionary_size; ++i) {
        unsigned char byte;
        uint16_t code_length; // Используем uint16_t для длины кода
        file.read(reinterpret_cast<char*>(&byte), sizeof(byte));
        file.read(reinterpret_cast<char*>(&code_length), sizeof(code_length));
        std::vector<char> code_buffer(code_length);
        file.read(code_buffer.data(), code_length);
        dictionary[byte] = std::string(code_buffer.begin(), code_buffer.end());
    }

    file.close();
    std::cout << "Словарь успешно загружен из файла: " << filename << std::endl;
    return dictionary;
}

/**
 * @brief Кодирует файл с использованием словаря Шеннона.
 * @param input_filename Имя входного файла.
 * @return true, если кодирование успешно.
 */
bool encode_file(const std::string& input_filename) {
    std::ifstream inputFile(input_filename, std::ios::binary);
    if (!inputFile.is_open()) {
        std::cerr << "Не удалось открыть входной файл: " << input_filename << std::endl;
        return false;
    }

    // Читаем содержимое файла для подсчета частот
    std::map<unsigned char, int> frequency = calculate_frequency(input_filename);

    // Строим словарь Шеннона
    std::map<unsigned char, std::string> dictionary = build_shannon_dictionary(frequency);

    // Генерация случайного ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    unsigned char id = static_cast<unsigned char>(distrib(gen));

    // Получаем имя файла без пути и расширения
    fs::path input_path = input_filename;
    std::string original_name = input_path.stem().string();
    std::string original_format = input_path.extension().string();

    // Формируем имя файла для словаря
    std::string dictionary_filename = "work/dictionary/dict_" + std::to_string(static_cast<int>(id)) + "_" + original_name + ".bin";

    // Сохраняем словарь
    if (!save_dictionary(dictionary, dictionary_filename, id)) {
        std::cerr << "Ошибка при сохранении словаря." << std::endl;
        return false;
    }

    // Формируем имя выходного закодированного файла
    std::string encoded_filename = "work/encoded/encoded_" + std::to_string(static_cast<int>(id)) + "_" + original_name + original_format;
    std::ofstream encodedFile(encoded_filename, std::ios::binary);
    if (!encodedFile.is_open()) {
        std::cerr << "Не удалось открыть файл для записи закодированных данных: " << encoded_filename << std::endl;
        return false;
    }

    // Записываем служебные байты в начало закодированного файла
    unsigned char dictionary_size = static_cast<unsigned char>(dictionary.size());
    encodedFile.write(reinterpret_cast<const char*>(&dictionary_size), sizeof(dictionary_size));
    encodedFile.write(reinterpret_cast<const char*>(&id), sizeof(id));

    // Кодируем файл
    inputFile.clear(); // Сбрасываем флаги ошибок
    inputFile.seekg(0, std::ios::beg); // Переходим в начало файла

    unsigned char byte;
    std::string current_code;
    unsigned char current_byte = 0;
    //int bits_in_byte = 0;

    while (inputFile.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        if (dictionary.count(byte)) {
            current_code += dictionary[byte];
            while (current_code.length() >= 8) {
                std::string byte_str = current_code.substr(0, 8);
                current_code.erase(0, 8);
                current_byte = 0;
                for (int i = 0; i < 8; ++i) {
                    if (byte_str[i] == '1') {
                        // Устанавливаем бит в current_byte на позиции (7 - i), если соответствующий бит в byte_str равен '1'
                        // Пример: если i = 2, то 1 << (7 - 2) = 1 << 5 = 00100000 
                        // Если current_byte = 10000000, то после выполнения операции:
                        // current_byte = 10000000 | 00100000 = 10100000
                        // коротко - побитовое или + сдвиг битов
                        current_byte |= (1 << (7 - i));
                    }
                }
                encodedFile.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte));
            }
        } else {
            std::cerr << "Ошибка: байт 0x" << std::hex << static_cast<int>(byte) << std::dec << " не найден в словаре." << std::endl;
            encodedFile.close();
            inputFile.close();
            return false;
        }
    }

    // Записываем оставшиеся биты (дополняем нулями до полного байта)
    if (!current_code.empty()) {
        while (current_code.length() < 8) {
            current_code += '0';
        }
        current_byte = 0;
        for (int i = 0; i < 8; ++i) {
            if (current_code[i] == '1') {
                current_byte |= (1 << (7 - i));
            }
        }
        encodedFile.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte));
    }

    encodedFile.close();
    inputFile.close();
    std::cout << "Файл успешно закодирован в: " << encoded_filename << std::endl;
    std::cout << "Словарь сохранен в: " << dictionary_filename << std::endl;
    return true;
}

/**
 * @brief Отображает список файлов в указанной директории.
 * @param directory Директория для отображения файлов.
 */
void list_files(const std::string& directory) {
    std::cout << "Доступные файлы в '" << directory << "':" << std::endl;
    int i = 1;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::cout << i++ << ". " << entry.path().filename().string() << std::endl;
        }
    }
    std::cout << "0. Выход" << std::endl;
}

