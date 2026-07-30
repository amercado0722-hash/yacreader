#include "search_field_registry.h"

#include <QCoreApplication>

#include <algorithm>
#include <cctype>

namespace {

QString fieldText(const char *text)
{
    return QCoreApplication::translate("SearchFieldRegistry", text);
}

QString inputDescription(SearchFieldType type)
{
    switch (type) {
    case SearchFieldType::Text:
    case SearchFieldType::Filename:
    case SearchFieldType::Folder:
        return fieldText("Text, quoted text");
    case SearchFieldType::Numeric:
        return fieldText("Integer");
    case SearchFieldType::Boolean:
    case SearchFieldType::BooleanFolder:
        return fieldText("Boolean (true / false)");
    case SearchFieldType::Date:
    case SearchFieldType::DateFolder:
        return fieldText("Integer (number of days)");
    case SearchFieldType::EnumField:
    case SearchFieldType::EnumFieldFolder:
        return fieldText("Enum (comic, manga, westernmanga, webcomic/web, 4koma/yonkoma)");
    case SearchFieldType::Unknown:
        return { };
    }

    return { };
}

SearchFieldDefinition field(
        const char *key,
        const char *displayName,
        const char *description,
        const char *example,
        SearchFieldType type,
        SearchFieldCategory category,
        SearchFieldScope scope = SearchFieldScope::Comics)
{
    return {
        QString::fromLatin1(key),
        fieldText(displayName),
        fieldText(description),
        inputDescription(type),
        QString::fromLatin1(example),
        type,
        category,
        scope
    };
}

}

const std::map<SearchFieldType, std::vector<std::string>> &searchFieldNames()
{
    static const std::map<SearchFieldType, std::vector<std::string>> names {
        { SearchFieldType::Numeric, { "numpages", "count", "arccount", "alternateCount", "rating" } },
        { SearchFieldType::Text, { "date", "number", "arcnumber", "title", "volume", "storyarc", "genere", "writer", "penciller", "inker", "colorist", "letterer", "coverartist", "publisher", "format", "agerating", "synopsis", "characters", "notes", "editor", "imprint", "teams", "locations", "series", "alternateSeries", "alternateNumber", "languageISO", "seriesGroup", "mainCharacterOrTeam", "review", "tags" } },
        { SearchFieldType::Boolean, { "color", "read", "edited", "hasBeenOpened" } },
        { SearchFieldType::Date, { "added", "lastTimeOpened" } },
        { SearchFieldType::DateFolder, { "added", "updated" } },
        { SearchFieldType::Filename, { "filename" } },
        { SearchFieldType::Folder, { "folder" } },
        { SearchFieldType::BooleanFolder, { "completed", "finished" } },
        { SearchFieldType::EnumField, { "type" } },
        { SearchFieldType::EnumFieldFolder, { "foldertype" } }
    };

    return names;
}

SearchFieldType searchFieldType(const std::string &name)
{
    std::string lowerName(name);
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    for (const auto &[type, names] : searchFieldNames()) {
        for (const auto &candidate : names) {
            std::string lowerCandidate(candidate);
            std::transform(lowerCandidate.begin(), lowerCandidate.end(), lowerCandidate.begin(), ::tolower);
            if (lowerCandidate == lowerName)
                return type;
        }
    }

    return SearchFieldType::Unknown;
}

const QList<SearchFieldDefinition> &searchFieldDefinitions()
{
    using Category = SearchFieldCategory;
    using Scope = SearchFieldScope;
    using Type = SearchFieldType;

    static const QList<SearchFieldDefinition> definitions {
        field("title", "Title", "Comic title", "title:Moonbound", Type::Text, Category::Common),
        field("series", "Series", "Series name", "series:\"Starfall Chronicles\"", Type::Text, Category::Common),
        field("number", "Number", "Issue number", "number>=10", Type::Text, Category::Common),
        field("volume", "Volume", "Volume identifier", "volume:2", Type::Text, Category::Common),
        field("type", "Comic type", "Reading format", "type:manga", Type::EnumField, Category::Common),
        field("rating", "Rating", "Comic rating", "rating>=4", Type::Numeric, Category::Common),
        field("tags", "Tags", "Textual tags", "tags:\"to review\"", Type::Text, Category::Common),

        field("writer", "Writer", "Writer credit", "writer:Smith", Type::Text, Category::Credits),
        field("penciller", "Penciller", "Penciller credit", "penciller:\"Alex Smith\"", Type::Text, Category::Credits),
        field("inker", "Inker", "Inker credit", "inker:\"Taylor Reed\"", Type::Text, Category::Credits),
        field("colorist", "Colorist", "Colorist credit", "colorist:\"Morgan Lane\"", Type::Text, Category::Credits),
        field("letterer", "Letterer", "Letterer credit", "letterer:\"Casey Brooks\"", Type::Text, Category::Credits),
        field("coverartist", "Cover artist", "Cover artist credit", "coverartist:\"Jordan Blake\"", Type::Text, Category::Credits),
        field("editor", "Editor", "Editor credit", "editor:\"Avery Stone\"", Type::Text, Category::Credits),

        field("storyarc", "Story arc", "Story arc name", "storyarc:Afterlight", Type::Text, Category::Story),
        field("arcnumber", "Arc number", "Position within a story arc", "arcnumber>=2", Type::Text, Category::Story),
        field("arccount", "Arc count", "Number of issues in a story arc", "arccount>5", Type::Numeric, Category::Story),
        field("characters", "Characters", "Characters appearing in the comic", "characters:Solara", Type::Text, Category::Story),
        field("teams", "Teams", "Teams appearing in the comic", "teams:\"Aurora Guard\"", Type::Text, Category::Story),
        field("locations", "Locations", "Locations appearing in the comic", "locations:Greyhaven", Type::Text, Category::Story),
        field("mainCharacterOrTeam", "Main character or team", "Primary character or team", "mainCharacterOrTeam:Solara", Type::Text, Category::Story),
        field("synopsis", "Synopsis", "Comic synopsis", "synopsis:\"parallel world\"", Type::Text, Category::Story),

        field("publisher", "Publisher", "Publisher name", "publisher:ExamplePress", Type::Text, Category::Publication),
        field("imprint", "Imprint", "Publishing imprint", "imprint:\"Silver Line\"", Type::Text, Category::Publication),
        field("format", "Format", "Publication format", "format:annual", Type::Text, Category::Publication),
        field("agerating", "Age rating", "Recommended age rating", "agerating:Teen", Type::Text, Category::Publication),
        field("genere", "Genre", "Comic genre", "genere:Horror", Type::Text, Category::Publication),
        field("languageISO", "Language", "ISO language code", "languageISO:en", Type::Text, Category::Publication),
        field("date", "Publication date", "Publication date metadata", "date:2024", Type::Text, Category::Publication),
        field("seriesGroup", "Series group", "Series grouping metadata", "seriesGroup:\"Pocket Editions\"", Type::Text, Category::Publication),
        field("alternateSeries", "Alternate series", "Alternate series name", "alternateSeries:\"Midnight Tales\"", Type::Text, Category::Publication),
        field("alternateNumber", "Alternate number", "Alternate issue number", "alternateNumber>=10", Type::Text, Category::Publication),
        field("alternateCount", "Alternate count", "Alternate series issue count", "alternateCount>20", Type::Numeric, Category::Publication),
        field("count", "Issue count", "Number of issues in the series", "count>=12", Type::Numeric, Category::Publication),

        field("read", "Read", "Whether the comic is marked as read", "read:false", Type::Boolean, Category::ReadingAndFiles),
        field("hasBeenOpened", "Has been opened", "Whether reading has started", "hasBeenOpened:true", Type::Boolean, Category::ReadingAndFiles),
        field("edited", "Edited", "Whether metadata has been edited", "edited:true", Type::Boolean, Category::ReadingAndFiles),
        field("color", "Color", "Whether the comic is in color", "color:true", Type::Boolean, Category::ReadingAndFiles),
        field("numpages", "Page count", "Number of pages", "numpages>100", Type::Numeric, Category::ReadingAndFiles),
        field("filename", "File name", "Comic file name", "filename:cbz", Type::Filename, Category::ReadingAndFiles),
        field("added", "Date added", "When the item was added", "added>7", Type::Date, Category::ReadingAndFiles, Scope::ComicsAndFolders),
        field("lastTimeOpened", "Last opened", "When the comic was last opened", "lastTimeOpened>30", Type::Date, Category::ReadingAndFiles),
        field("notes", "Notes", "Comic notes", "notes:\"variant cover\"", Type::Text, Category::ReadingAndFiles),
        field("review", "Review", "Review text", "review:\"highly recommended\"", Type::Text, Category::ReadingAndFiles),

        field("folder", "Folder", "Parent folder name", "folder:\"Example Comics\"", Type::Folder, Category::Folders, Scope::Folders),
        field("foldertype", "Folder type", "Default reading format for the folder", "foldertype:manga", Type::EnumFieldFolder, Category::Folders, Scope::Folders),
        field("completed", "Completed", "Whether the folder is complete", "completed:true", Type::BooleanFolder, Category::Folders, Scope::Folders),
        field("finished", "Finished", "Whether the folder is marked as finished", "finished:true", Type::BooleanFolder, Category::Folders, Scope::Folders),
        field("updated", "Date updated", "When the folder was updated", "updated>7", Type::DateFolder, Category::Folders, Scope::Folders)
    };

    return definitions;
}
