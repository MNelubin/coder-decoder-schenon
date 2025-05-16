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
 *
 * Эта функция принимает карту частот байтов и строит словарь кодов Шеннона.
 * Коды генерируются на основе кумулятивных вероятностей символов, отсортированных по убыванию частоты.
 *
 * @param frequency_map Карта частот байтов, где ключ - байт, значение - его частота.
 * @return Словарь Шеннона, где ключ - байт, значение - его код в виде строки из '0' и '1'.
 */
std::map<unsigned char, std::string> build_shannon_dictionary(const std::map<unsigned char, int>& frequency_map) {
    std::map<unsigned char, std::string> dictionary;

    // Если карта частот пуста, возвращаем пустой словарь
    if (frequency_map.empty()) {
        return dictionary;
    }

    // Преобразуем карту частот в вектор пар для сортировки
    std::vector<std::pair<unsigned char, int>> sorted_frequencies(frequency_map.begin(), frequency_map.end());

    // Сортируем частоты по убыванию
    std::sort(sorted_frequencies.begin(), sorted_frequencies.end(), compareFrequency);

    size_t n = sorted_frequencies.size();

    // Если только один тип символа, присваиваем ему код "0"
    if (n == 1) {
        dictionary[sorted_frequencies[0].first] = "0";
        return dictionary;
    }

    std::vector<double> probabilities(n);
    double total_frequency = 0.0;

    // Подсчет общего количества встреч байтов
    for (const auto& p : sorted_frequencies) {
        total_frequency += p.second;
    }

    // Если общая частота равна нулю (хотя при n > 0 это маловероятно), возвращаем пустой словарь
    if (total_frequency == 0.0) {
        return dictionary;
    }

    // Вычисляем вероятности для каждого символа
    for (size_t i = 0; i < n; ++i) {
        probabilities[i] = static_cast<double>(sorted_frequencies[i].second) / total_frequency;
    }

    // Вычисляем кумулятивные вероятности
    std::vector<double> cumulative_probabilities(n);
    cumulative_probabilities[0] = 0.0;
    for (size_t i = 1; i < n; ++i) {
        cumulative_probabilities[i] = cumulative_probabilities[i - 1] + probabilities[i - 1];
    }

    // Генерируем коды Шеннона
    for (size_t i = 0; i < n; ++i) {
        std::string code_str = ""; // Инициализируем пустую строку для кода
        double p_val = cumulative_probabilities[i]; // Начальное значение кумулятивной вероятности

        int code_length;
        // Пропускаем символы с нулевой вероятностью 
        if (probabilities[i] <= 0) {
            continue;
        }

        // Вычисляем длину кода: ceil(-log2(P(x)))
        // Для n > 1, вероятность не может быть 1.0 (т.к. есть другие символы)
        // Но если она очень близка к 1, log2 может дать почти 0.
        code_length = std::ceil(-std::log2(probabilities[i]));

        // Минимальная длина кода - 1 бит
        if (code_length == 0) {
            code_length = 1;
        }

        // Генерируем код Шеннона
        for (int j = 0; j < code_length; ++j) {
            p_val *= 2.0; // Удваиваем значение p_val
            if (p_val >= 1.0) {
                code_str += '1'; // Добавляем '1' к коду
                p_val -= 1.0; // Вычитаем 1.0 из p_val
            } else {
                code_str += '0'; // Добавляем '0' к коду
            }
        }

        // Сохраняем сгенерированный код в словаре, если он не пустой
        if (!code_str.empty()) {
            dictionary[sorted_frequencies[i].first] = code_str;
        } else {
            std::cerr << "Предупреждение: Сгенерирован пустой код для символа с ID " << static_cast<int>(sorted_frequencies[i].first) << std::endl;
        }
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
 *
 * Эта функция открывает входной файл, вычисляет частоты байтов, строит словарь Шеннона,
 * сохраняет словарь в отдельный файл и кодирует входной файл, записывая закодированные
 * данные в выходной файл. В начало закодированного файла записывается служебная информация:
 * исходный размер файла, размер словаря и случайный ID.
 *
 * @param input_filename Имя входного файла для кодирования.
 * @return true, если кодирование успешно, false в противном случае.
 */
bool encode_file(const std::string& input_filename) {
    // Открываем файл для получения размера и затем возвращаемся в начало
    std::ifstream inputFile(input_filename, std::ios::binary | std::ios::ate);
    if (!inputFile.is_open()) {
        std::cerr << "Не удалось открыть входной файл: " << input_filename << std::endl;
        return false;
    }
    std::streamsize original_size_ss = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg); // Возвращаемся в начало для чтения содержимого

    // Преобразуем размер файла в uint64_t
    uint64_t original_file_size = static_cast<uint64_t>(original_size_ss);

    // Вычисляем частоты байтов и строим словарь Шеннона
    std::map<unsigned char, int> frequency = calculate_frequency(input_filename);
    std::map<unsigned char, std::string> dictionary = build_shannon_dictionary(frequency);


    // Генерируем случайный ID для связывания закодированного файла и словаря
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    unsigned char id = static_cast<unsigned char>(distrib(gen));

    // Получаем имя файла без пути и расширения для формирования имен выходных файлов
    fs::path input_path = input_filename;
    std::string original_name = input_path.stem().string();
    std::string original_format = input_path.extension().string();

    // Формируем имя файла для словаря
    std::string dictionary_filename = "work/dictionary/dict_" + std::to_string(static_cast<int>(id)) + "_" + original_name + ".bin";

    // Сохраняем словарь в файл
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
    // 1. Записываем исходный размер файла (uint64_t - 8 байт)
    encodedFile.write(reinterpret_cast<const char*>(&original_file_size), sizeof(original_file_size));
    // 2. Записываем размер словаря (количество записей в map) - 1 байт
    unsigned char dictionary_map_size = static_cast<unsigned char>(dictionary.size());
    encodedFile.write(reinterpret_cast<const char*>(&dictionary_map_size), sizeof(dictionary_map_size));
    // 3. Записываем ID (1 байт)
    encodedFile.write(reinterpret_cast<const char*>(&id), sizeof(id));

    // Если исходный файл пуст, больше ничего не пишем в закодированный файл
    if (original_file_size == 0) {
        encodedFile.close();
        inputFile.close();
        std::cout << "Файл (пустой) успешно закодирован в: " << encoded_filename << std::endl;
        std::cout << "Словарь (пустой) сохранен в: " << dictionary_filename << std::endl;
        return true;
    }

    // Кодируем файл
    unsigned char byte;
    std::string current_code_buffer; // Буфер для накопления битов кода
    unsigned char current_byte_to_write = 0; // Байт, который будет записан в файл

    // Читаем файл побайтово и кодируем
    while (inputFile.read(reinterpret_cast<char*>(&byte), sizeof(byte))) {
        // Проверяем, есть ли байт в словаре
        if (dictionary.count(byte)) {
            current_code_buffer += dictionary[byte]; // Добавляем код байта в буфер
            // Пока в буфере достаточно битов для формирования полного байта (8 и более)
            while (current_code_buffer.length() >= 8) {
                std::string byte_str = current_code_buffer.substr(0, 8); // Берем первые 8 битов
                current_code_buffer.erase(0, 8); // Удаляем эти 8 битов из буфера
                current_byte_to_write = 0; // Сбрасываем байт для записи
                // Преобразуем строку из '0' и '1' в байт
                for (int i = 0; i < 8; ++i) {
                    if (byte_str[i] == '1') {
                        // Устанавливаем бит в current_byte на позиции (7 - i), если соответствующий бит в byte_str равен '1'
                        // Пример: если i = 2, то 1 << (7 - 2) = 1 << 5 = 00100000 
                        // Если current_byte = 10000000, то после выполнения операции:
                        // current_byte = 10000000 | 00100000 = 10100000
                        // коротко - побитовое или + сдвиг битов
                        current_byte_to_write |= (1 << (7 - i));
                    }
                }
                encodedFile.write(reinterpret_cast<const char*>(&current_byte_to_write), sizeof(current_byte_to_write));
            }
        } else {
            std::cerr << "Ошибка: байт 0x" << std::hex << static_cast<int>(byte) << std::dec << " не найден в словаре." << std::endl;
            encodedFile.close();
            inputFile.close();
            return false;
        }
    }

    // Записываем оставшиеся биты, если они есть (дополняем нулями до полного байта)
    if (!current_code_buffer.empty()) {
        while (current_code_buffer.length() < 8) {
            current_code_buffer += '0'; // Дополняем нулями
        }
        current_byte_to_write = 0; // Сбрасываем байт для записи
        // Преобразуем оставшиеся биты в байт
        for (int i = 0; i < 8; ++i) {
            if (current_code_buffer[i] == '1') {
                current_byte_to_write |= (1 << (7 - i));
            }
        }
        // Записываем последний байт
        encodedFile.write(reinterpret_cast<const char*>(&current_byte_to_write), sizeof(current_byte_to_write));
    }

    // Закрываем файлы и выводим сообщение об успехе
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

