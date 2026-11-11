#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }
// Перечитывает settings.json с диска и обновляет параметры игры при перезапуске партии.

    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }
// Оператор () позволяет получить настройку как config("Раздел", "Имя");
// возвращаемое значение берётся из загруженного JSON.

    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
