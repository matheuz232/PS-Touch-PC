#pragma once
#include "pstouch/image.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace pstouch {
enum class BlendMode : uint8_t { Normal=0, Darken=1, Multiply=2, Lighten=3, Screen=4, Add=5, Overlay=6, Difference=7, Subtract=8 };
struct Layer {
    std::string name;
    Image image;
    int32_t x{0}, y{0};
    uint8_t opacity{255};
    bool visible{true};
    BlendMode blend{BlendMode::Normal};
    Layer(std::string name, Image image);
};
class Document {
public:
    Document(uint32_t width, uint32_t height, std::string name="Untitled");
    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }
    const std::string& name() const noexcept { return name_; }
    void set_name(std::string name);
    const std::vector<Layer>& layers() const noexcept { return layers_; }
    std::vector<Layer>& mutable_layers() noexcept { return layers_; }
    size_t add_layer(Layer layer);
    void remove_layer(size_t index);
    void move_layer(size_t from, size_t to);
    void rename_layer(size_t index, std::string name);
    void set_layer_visibility(size_t index, bool visible);
    void set_layer_opacity(size_t index, uint8_t opacity);
    size_t duplicate_layer(size_t index);
    Image composite() const;
    // Rotate the complete canvas and all layer pixels/offsets without clipping.
    void rotate_canvas(bool clockwise);
    void save(const std::string& path) const;
    static Document load(const std::string& path);
    void checkpoint(std::string label);
    bool undo(); bool redo();
    bool can_undo() const noexcept; bool can_redo() const noexcept;
    const std::string& undo_label() const noexcept;
    const std::string& redo_label() const noexcept;
private:
    struct State { std::string label; uint32_t width{},height{}; std::string name; std::vector<Layer> layers; };
    State snapshot(std::string label) const;
    void restore(const State& state);
    uint32_t width_,height_; std::string name_; std::vector<Layer> layers_;
    std::vector<State> history_; size_t history_cursor_{0}; static constexpr size_t kMaxHistory=21;
};
}
