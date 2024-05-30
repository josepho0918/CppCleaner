﻿#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

using namespace std;
using namespace std::filesystem;

static const path dir_path = current_path();
static mutex mut;

static bool IsWhiteSpace(char ch)
{
    return isspace(static_cast<unsigned char>(ch));
}

static char ToLowerCase(char ch)
{
    return tolower(static_cast<unsigned char>(ch));
}

static bool HasTrailingBlanks(ifstream& file)
{
    streampos pos = file.tellg();
    bool result = false;
    string line;

    file.seekg(0);

    while (getline(file, line)) {
        if (!line.empty() && IsWhiteSpace(line.back())) {
            result = true;
            break;
        }
    }

    file.clear();
    file.seekg(pos);

    return result;
}

static optional<string> GetCleanLine(ifstream& file)
{
    if (string line; getline(file, line)) {
        line.erase(find_if_not(line.crbegin(), line.crend(), IsWhiteSpace).base(), line.cend());
        return line;
    }

    return nullopt;
}

static void PrintFile(string_view sv)
{
    sv.remove_prefix(dir_path.string().size() + 1);
    mut.lock();
    cout << sv << endl;
    mut.unlock();
}

static void RemoveTrailingBlanks(const path& file_path)
{
    if (ifstream orig_file(file_path); HasTrailingBlanks(orig_file)) {
        char temp_path[L_tmpnam_s];
        tmpnam_s(temp_path);
        ofstream temp_file(temp_path);

        if (!temp_file.is_open()) {
            return;
        }

        while (auto line = GetCleanLine(orig_file)) {
            temp_file << *line;
            if (!orig_file.eof()) {
                temp_file << '\n';
            }
        }

        orig_file.close();
        temp_file.close();
        rename(temp_path, file_path);
    }

    PrintFile(file_path.string());
}

static void ProcessDir(const unordered_set<string>& allowed_exts)
{
    vector<path> file_paths;

    cout << "Processing directory: " << dir_path.string() << endl;

    for (const auto& file : recursive_directory_iterator(dir_path, directory_options::skip_permission_denied)) {
        if (file.is_regular_file()) {
            string file_ext = file.path().extension().string();
            ranges::transform(file_ext, file_ext.begin(), ToLowerCase);
            if (allowed_exts.contains(file_ext)) {
                file_paths.push_back(file.path());
            }
        }
    }

    for_each(execution::par, file_paths.cbegin(), file_paths.cend(), RemoveTrailingBlanks);
}

int main(int argc, char* argv[])
{
    auto start = chrono::high_resolution_clock::now();
    unordered_set<string> allowed_exts;

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            string str = argv[i];
            ranges::transform(str, str.begin(), ToLowerCase);
            allowed_exts.insert(str);
        }
    }
    else {
        allowed_exts = { ".h", ".c", ".hpp", ".cpp" };
    }

    ProcessDir(allowed_exts);

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    cout << "Elapsed time: " << duration.count() << " ms" << endl;

    return 0;
}