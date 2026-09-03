#pragma once
#include <string>
#include <functional>
#include <SDCardManager.h>
#include <Xtc/BibleBooks.h>
#include <GfxRenderer.h>

#include "Settings.h"
#include "Battery.h"
#include "activities/Activity.h"

// Grid-based book picker for the Bible. Reached directly from the "Библия"
// header shortcut in FileSelection (which always jumps straight here for
// whichever translation was last used — NKJV or RSV are never shown as a
// browsable folder list). Displays all 66 books as a 6x11 grid (6 columns,
// 11 rows) of shorthand labels, with two extra always-visible selectable
// header slots:
//   - center: "Библия" (returns to the root library)
//   - top-left: the current translation, e.g. "NKJV" / "RSV" (toggles it)
//
// Selection state model:
//   - HeaderState::Grid    -> a (row, col) cell in the 6x11 grid is active
//   - HeaderState::Bible   -> the center "Библия" label is active
//   - HeaderState::Version -> the top-left version label is active
//
// Navigation:
//   Grid(0, col) + Up      -> Bible
//   Grid(row>0, col) + Up  -> Grid(row-1, col)
//   Grid(row<10, col) + Down -> Grid(row+1, col)
//   Grid: Left/Right clamp at row edges (no wrap)
//   Bible + Left   -> Version
//   Bible + Right  -> (no-op)
//   Bible + Down   -> Grid(0, lastCol)
//   Bible + Confirm -> exits to the root library (via onGoToLibraryRoot)
//   Version + Right -> Bible
//   Version + Down  -> Grid(0, lastCol)
//   Version + Confirm -> toggles NKJV <-> RSV in place
//
// The last-selected grid cell (row/col) is remembered across version
// toggles and re-entry into the grid for the lifetime of this activity.
class BibleGridActivity final : public Activity {
public:
    static constexpr int COLS = 6;
    static constexpr int ROWS = 11;
    static_assert(COLS * ROWS == 66, "grid must have exactly 66 cells for the 66 books");

private:
    enum class Selection { Grid, Bible, Version };

    const std::function<void(const std::string&)> onSelectBook;
    const std::function<void()> onGoToLibraryRoot;

    Selection selection = Selection::Grid;
    int gridRow = 0;
    int gridCol = 0;
    int lastCol = 0;  // remembered column when leaving the grid via Up

    bool updateRequired = true;

    static constexpr const char* BIBLE_LABEL = "\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F"; // "Библия"
    static constexpr const char* NKJV_FOLDER = "NKJV";
    static constexpr const char* RSV_FOLDER = "RSV";
    static constexpr const char* BIBLE_ROOT_PATH = "/\xD0\x91\xD0\xB8\xD0\xB1\xD0\xBB\xD0\xB8\xD1\x8F"; // "/Библия"

    bool isRsv() const { return SETTINGS.getValue(SETTING_BIBLE_VERSION) == 1; }

    const char* currentVersionLabel() const {
        return SETTINGS.getValueLabel(SETTING_BIBLE_VERSION);
    }

    std::string currentVersionFolder() const {
        return isRsv() ? RSV_FOLDER : NKJV_FOLDER;
    }

    static int bookIndex(int row, int col) { return row * COLS + col; }

    const char* bookShortLabel(int index) const {
        return isRsv() ? BIBLE_BOOKS[index].shortRu : BIBLE_BOOKS[index].shortEn;
    }

    std::string bookFilePath(int index) const {
        const auto& book = BIBLE_BOOKS[index];
        const std::string name = isRsv() ? book.nameRu : book.nameEn;
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%02u", book.number);
        return std::string(BIBLE_ROOT_PATH) + "/" + currentVersionFolder() + "/" +
               numBuf + " " + name + ".xtc";
    }

    void toggleVersion() {
        SETTINGS.setValue(SETTING_BIBLE_VERSION, isRsv() ? 0 : 1);
        updateRequired = true;
    }

    void render() const {
        renderer.clearScreen();

        const int pageWidth = renderer.getScreenWidth();
        const int pageHeight = renderer.getScreenHeight();

        // --- Center header: "Библия" ---
        const bool bibleSelected = (selection == Selection::Bible);
        {
            const int textWidth = renderer.getTextWidth(GfxRenderer::LARGE, BIBLE_LABEL);
            const int x = (pageWidth - textWidth) / 2;
            renderer.drawText(GfxRenderer::LARGE, x, 15, BIBLE_LABEL, true);
            if (bibleSelected) {
                const int underlineY = 15 + renderer.getFontAscenderSize(GfxRenderer::LARGE) + 2;
                renderer.drawLine(x, underlineY, x + textWidth, underlineY);
            }
        }

        // --- Top-left header: current version label ---
        const bool versionSelected = (selection == Selection::Version);
        {
            const char* label = currentVersionLabel();
            const int x = 15;
            renderer.drawText(GfxRenderer::LARGE, x, 15, label, true);
            if (versionSelected) {
                const int textWidth = renderer.getTextWidth(GfxRenderer::LARGE, label);
                const int underlineY = 15 + renderer.getFontAscenderSize(GfxRenderer::LARGE) + 2;
                renderer.drawLine(x, underlineY, x + textWidth, underlineY);
            }
        }

        // --- Grid of 66 books ---
        const int gridTop = 70;
        const int gridBottom = pageHeight - 50;  // leave room for battery indicator
        const int cellWidth = pageWidth / COLS;
        const int cellHeight = (gridBottom - gridTop) / ROWS;

        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                const int index = bookIndex(row, col);
                const int cellX = col * cellWidth;
                const int cellY = gridTop + row * cellHeight;
                const bool isSelected = (selection == Selection::Grid) && row == gridRow && col == gridCol;

                const char* label = bookShortLabel(index);
                const int textWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, label);
                const int textX = cellX + (cellWidth - textWidth) / 2;
                const int textY = cellY + (cellHeight - renderer.getLineHeight(GfxRenderer::MEDIUM)) / 2;

                if (isSelected) {
                    renderer.fillRect(cellX, cellY, cellWidth, cellHeight);
                }
                renderer.drawText(GfxRenderer::MEDIUM, textX, textY, label, !isSelected);
            }
        }

        // --- Battery indicator ---
        const uint16_t batteryPercentage = battery.readPercentage();
        const std::string batteryText = std::to_string(batteryPercentage) + "%";
        renderer.drawCenteredText(GfxRenderer::SMALL, pageHeight - 30, batteryText.c_str());

        renderer.displayBuffer();
    }

public:
    explicit BibleGridActivity(GfxRenderer& renderer, InputManager& inputManager,
                               const std::function<void(const std::string&)>& onSelectBook,
                               const std::function<void()>& onGoToLibraryRoot)
        : Activity("BibleGridActivity", renderer, inputManager),
          onSelectBook(onSelectBook),
          onGoToLibraryRoot(onGoToLibraryRoot)
    {}

    void onEnter() override {
        Activity::onEnter();
        render();
    }

    void loop() override {
        if (inputManager.wasReleased(InputManager::Button::Back)) {
            onGoToLibraryRoot();
            return;
        }

        const bool up = inputManager.wasReleased(InputManager::Button::Up);
        const bool down = inputManager.wasReleased(InputManager::Button::Down);
        const bool left = inputManager.wasReleased(InputManager::Button::Left);
        const bool right = inputManager.wasReleased(InputManager::Button::Right);
        const bool confirm = inputManager.wasReleased(InputManager::Button::Confirm);

        if (!up && !down && !left && !right && !confirm) return;

        switch (selection) {
            case Selection::Grid: {
                if (confirm) {
                    onSelectBook(bookFilePath(bookIndex(gridRow, gridCol)));
                    return;
                }
                if (up) {
                    if (gridRow == 0) {
                        lastCol = gridCol;
                        selection = Selection::Bible;
                    } else {
                        gridRow--;
                    }
                    updateRequired = true;
                } else if (down) {
                    if (gridRow < ROWS - 1) {
                        gridRow++;
                        updateRequired = true;
                    }
                } else if (left) {
                    if (gridCol > 0) {
                        gridCol--;
                        updateRequired = true;
                    }
                } else if (right) {
                    if (gridCol < COLS - 1) {
                        gridCol++;
                        updateRequired = true;
                    }
                }
                break;
            }

            case Selection::Bible: {
                if (confirm) {
                    onGoToLibraryRoot();
                    return;
                }
                if (left) {
                    selection = Selection::Version;
                    updateRequired = true;
                } else if (down) {
                    gridRow = 0;
                    gridCol = lastCol;
                    selection = Selection::Grid;
                    updateRequired = true;
                }
                // Right: no-op
                break;
            }

            case Selection::Version: {
                if (confirm) {
                    toggleVersion();
                } else if (right) {
                    selection = Selection::Bible;
                    updateRequired = true;
                } else if (down) {
                    gridRow = 0;
                    gridCol = lastCol;
                    selection = Selection::Grid;
                    updateRequired = true;
                }
                break;
            }
        }

        if (updateRequired) {
            render();
            updateRequired = false;
        }
    }

    void onExit() override {
        Activity::onExit();
    }
};