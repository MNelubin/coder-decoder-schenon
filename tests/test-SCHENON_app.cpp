#include "gtest/gtest.h"
#include "coder.h" 
#include "decoder.h" 

#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream> 
#include <algorithm> // Для std::equal

namespace fs = std::filesystem;

// --- Вспомогательные функции ---

/**
 * @brief Создает файл с заданным содержимым.
 * @param filepath Путь к файлу.
 * @param content Содержимое файла.
 */
void create_test_file(const std::string& filepath, const std::string& content) {
    // Убедимся, что директория для файла существует
    fs::path path_obj(filepath);
    if (path_obj.has_parent_path()) {
        fs::create_directories(path_obj.parent_path());
    }
    std::ofstream outfile(filepath, std::ios::binary);
    ASSERT_TRUE(outfile.is_open()) << "Не удалось создать тестовый файл: " << filepath;
    outfile << content;
    outfile.close();
}

/**
 * @brief Сравнивает два файла побайтово.
 * @param filepath1 Путь к первому файлу.
 * @param filepath2 Путь ко второму файлу.
 * @return true, если файлы идентичны, иначе false.
 */
bool compare_files(const std::string& filepath1, const std::string& filepath2) {
    std::ifstream file1(filepath1, std::ios::binary | std::ios::ate);
    std::ifstream file2(filepath2, std::ios::binary | std::ios::ate);

    if (!file1.is_open()) {
        std::cerr << "Не удалось открыть для сравнения файл: " << filepath1 << std::endl;
        return false;
    }
    if (!file2.is_open()) {
        std::cerr << "Не удалось открыть для сравнения файл: " << filepath2 << std::endl;
        return false;
    }

    if (file1.tellg() != file2.tellg()) {
        std::cerr << "Файлы имеют разный размер: "
                  << filepath1 << " (" << file1.tellg() << " байт) vs "
                  << filepath2 << " (" << file2.tellg() << " байт)" << std::endl;
        return false; // Разные размеры
    }

    file1.seekg(0, std::ios::beg);
    file2.seekg(0, std::ios::beg);

    return std::equal(std::istreambuf_iterator<char>(file1.rdbuf()),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(file2.rdbuf()));
}

/**
 * @brief Очищает указанную директорию (удаляет все файлы и поддиректории).
 * @param directory_path Путь к директории.
 */
void clear_directory(const std::string& directory_path) {
    if (!fs::exists(directory_path)) {
        fs::create_directories(directory_path); // Создаем, если не существует
        return;
    }
    for (const auto& entry : fs::directory_iterator(directory_path)) {
        fs::remove_all(entry.path());
    }
}

/**
 * @brief Находит первый файл в директории, имя которого соответствует шаблону "<prefix><id>_<base_name_part>.<extension_part>"
 * или для словарей "<prefix><id>_<base_name_part>.bin"
 * или для декодированных "<prefix><base_name_part>.<extension_part>"
 * @param dir Путь к директории.
 * @param prefix Префикс имени файла (например, "encoded_" или "dict_").
 * @param base_name_part Часть оригинального имени файла (например, "test_file").
 * @param extension_part Расширение оригинального файла с точкой (например, ".txt") или пустая строка для словарей.
 * @return Полный путь к найденному файлу или пустая строка.
 */
std::string find_matching_file(const std::string& dir, const std::string& prefix, const std::string& base_name_part, const std::string& extension_part = "") {
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            // Проверяем префикс
            if (filename.rfind(prefix, 0) == 0) {
                // Проверяем наличие части основного имени
                if (filename.find(base_name_part) != std::string::npos) {
                    // Проверяем наличие расширения (если оно указано)
                    if (extension_part.empty() || (filename.length() > extension_part.length() && filename.substr(filename.length() - extension_part.length()) == extension_part)) {
                         // Особый случай для словарей, у них расширение .bin
                        if (prefix == "dict_" && filename.substr(filename.length() - 4) != ".bin") {
                            continue;
                        }
                        return entry.path().string();
                    }
                }
            }
        }
    }
    return "";
}


// --- Тестовый класс ---
class ShannonCodingTest : public ::testing::Test {
protected:
    const std::string raw_dir_ = "work/raw/";
    const std::string dictionary_dir_ = "work/dictionary/";
    const std::string encoded_dir_ = "work/encoded/";
    const std::string decoded_dir_ = "work/decoded/";

    // Вызывается перед каждым тестом
    void SetUp() override {
        // Создаем директории, если их нет, и очищаем их
        fs::create_directories(raw_dir_);
        fs::create_directories(dictionary_dir_);
        fs::create_directories(encoded_dir_);
        fs::create_directories(decoded_dir_);

        clear_directory(raw_dir_);
        clear_directory(dictionary_dir_);
        clear_directory(encoded_dir_);
        clear_directory(decoded_dir_);
    }



    // Обобщенная функция для выполнения цикла кодирования-декодирования-сравнения
    void PerformEncodeDecodeTest(const std::string& base_filename, const std::string& file_content, const std::string& original_extension_with_dot) {
        const std::string original_filepath = raw_dir_ + base_filename + original_extension_with_dot;
        create_test_file(original_filepath, file_content);

        // 1. Кодирование
        ASSERT_TRUE(encode_file(original_filepath)) << "Ошибка кодирования файла: " << original_filepath;

        // 2. Поиск закодированного файла и файла словаря
        // Имя файла словаря: dict_<id>_<base_filename>.bin
        // Имя закодированного файла: encoded_<id>_<base_filename><original_extension_with_dot>
        std::string encoded_filepath = find_matching_file(encoded_dir_, "encoded_", base_filename, original_extension_with_dot);
        ASSERT_FALSE(encoded_filepath.empty()) << "Закодированный файл для '" << base_filename << "' не найден в " << encoded_dir_;

        std::string dictionary_filepath = find_matching_file(dictionary_dir_, "dict_", base_filename, ".bin");
        ASSERT_FALSE(dictionary_filepath.empty()) << "Файл словаря для '" << base_filename << "' не найден в " << dictionary_dir_;
        ASSERT_TRUE(fs::exists(dictionary_filepath)) << "Файл словаря не существует: " << dictionary_filepath;


        // 3. Декодирование
        ASSERT_TRUE(decode_file(encoded_filepath)) << "Ошибка декодирования файла: " << encoded_filepath;

        // 4. Поиск декодированного файла
        const std::string decoded_filepath = decoded_dir_ + "decoded_" + base_filename + original_extension_with_dot;
        ASSERT_TRUE(fs::exists(decoded_filepath)) << "Декодированный файл не существует: " << decoded_filepath;

        // 5. Сравнение
        ASSERT_TRUE(compare_files(original_filepath, decoded_filepath))
            << "Исходный файл (" << original_filepath << ") и декодированный файл (" << decoded_filepath << ") не идентичны.";
        
       
    }
};

// --- Тестовые случаи ---

TEST_F(ShannonCodingTest, EmptyFile) {
    PerformEncodeDecodeTest("empty_file", "", ".txt");
}

TEST_F(ShannonCodingTest, SingleCharacterFile) {
    PerformEncodeDecodeTest("single_char_file", "a", ".txt");
}

TEST_F(ShannonCodingTest, RepeatedCharactersFile) {
    PerformEncodeDecodeTest("repeated_chars_file", "aaaaabbbbbcccccddddd", ".txt");
}

TEST_F(ShannonCodingTest, SimpleTextFile) {
    PerformEncodeDecodeTest("simple_text_file", "Hello Shannon-Fano!", ".txt");
}

TEST_F(ShannonCodingTest, UniqueCharactersFile) {
    PerformEncodeDecodeTest("unique_chars_file", "abcdefghijklmnopqrstuvwxyz0123456789", ".dat");
}

TEST_F(ShannonCodingTest, MixedContentFile) {
    PerformEncodeDecodeTest(
        "mixed_content_file",
        "This is a test file with mixed content.\nIt includes newlines, tabs\t, and various symbols: !@#$%^&*()_+{}:\"<>?`~.\nAlso, numbers 1234567890 and repeated sequences aabbccddeeff.",
        ".log"
    );
}

TEST_F(ShannonCodingTest, AllBytesFile) {
    std::string content;
    for (int i = 0; i < 256; ++i) {
        content += static_cast<char>(i);
    }
    // Повторим несколько раз, чтобы было что сжимать
    std::string final_content;
    for(int i=0; i<5; ++i) final_content += content;

    PerformEncodeDecodeTest("all_bytes_file", final_content, ".bin");
}

