#include <fstream>
#include <sstream>

#include "portable-file-dialogs.h"

#include "Engine/Filesystem/Filesystem.hpp"

std::optional<Filepath> Filesystem::ShowOpenDialog(const DialogDescription& description)
{
    pfd::open_file openDialog(description.title,
            description.defaultPath.GetAbsolute(),
            description.filters, pfd::opt::none);

    if (!openDialog.result().empty())
    {
        return Filepath(openDialog.result().front());
    }

    return std::nullopt;
}

std::optional<Filepath> Filesystem::ShowSaveDialog(const DialogDescription& description)
{
    pfd::save_file saveDialog(description.title,
            description.defaultPath.GetAbsolute(),
            description.filters, pfd::opt::force_overwrite);

    if (!saveDialog.result().empty())
    {
        return Filepath(saveDialog.result());
    }

    return std::nullopt;
}

std::string Filesystem::ReadFile(const Filepath& filepath)
{
    const std::ifstream file(filepath.GetAbsolute());

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

std::vector<Filepath> Filesystem::GetFilepaths(const Filepath& folder, const std::string& extension)
{
    std::vector<Filepath> filepaths;

    for (const auto& entry : std::filesystem::directory_iterator(folder.Get()))
    {
        if (entry.path().extension().string() == extension)
        {
            filepaths.push_back(Filepath(entry.path().string()));
        }
    }

    return filepaths;
}
