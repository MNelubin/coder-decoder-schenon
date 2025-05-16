#include "coder.h"
#include "decoder.h"
#include <iostream>
#include <map>
#include <random> // Для генерации случайного байта


namespace fs = std::filesystem;

int main() {
    // Создаем необходимые директории, если они не существуют
    fs::create_directories("work/raw");
    fs::create_directories("work/dictionary");
    fs::create_directories("work/encoded");
    fs::create_directories("work/decoded");

    std::string raw_directory = "work/raw";
    std::string encoded_directory = "work/encoded";
    int choice;

    while (true) {
        
        std::cout << "\nВыберите действие:" << std::endl;
        std::cout << "1. Кодировать файл" << std::endl;
        std::cout << "2. Декодировать файл" << std::endl;
        std::cout << "0. Выход" << std::endl;
        std::cout << "Ваш выбор: ";
        std::cin >> choice;

        if (choice == 0) {
            break;
        }

        if (choice == 1) {
            std::cout << "\033[2J\033[1;1H";
            list_files(raw_directory);
            std::cout << "Выберите файл для кодирования (введите номер) или 0 для возврата: ";
            std::cin >> choice;

            if (choice == 0) {
                std::cout << "\033[2J\033[1;1H";
                continue;
            }

            int i = 1;
            for (const auto& entry : fs::directory_iterator(raw_directory)) {
                if (entry.is_regular_file()) {
                    if (i == choice) {
                        if (encode_file(entry.path().string())) {
                            std::cout << "Кодирование завершено." << std::endl;
                        } else {
                            std::cerr << "Ошибка при кодировании файла." << std::endl;
                        }
                        break;
                    }
                    i++;
                }
            }

            if (choice > 0 && i > choice) {
                std::cout << "Неверный номер файла." << std::endl;
            }
        } else if (choice == 2) {
            std::cout << "\033[2J\033[1;1H";
            list_encoded_files(encoded_directory);
            std::cout << "Выберите файл для декодирования (введите номер) или 0 для возврата: ";
            std::cin >> choice;

            if (choice == 0) {
                continue;
            }

            int i = 1;
            for (const auto& entry : fs::directory_iterator(encoded_directory)) {
                if (entry.is_regular_file() && entry.path().filename().string().find("encoded_") == 0) {
                    if (i == choice) {
                        if (decode_file(entry.path().string())) {
                            std::cout << "Декодирование завершено." << std::endl;
                        } else {
                            std::cerr << "Ошибка при декодировании файла." << std::endl;
                        }
                        break;
                    }
                    i++;
                }
            }

            if (choice > 0 && i > choice) {
                std::cout << "Неверный номер файла." << std::endl;
            }
        } else {
            std::cout << "Неверный выбор." << std::endl;
        }
    }

    std::cout << "Программа завершена." << std::endl;
    return 0;
}