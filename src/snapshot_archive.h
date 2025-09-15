#pragma once

#include <vector>
#include <cstdint>
#include <type_traits>
#include <cstring>
#include <string>
#include <entt/entt.hpp>

// A very small binary archive that writes to/reads from a byte vector.
// It fulfills the API required by entt snapshots/loaders.
class VectorOutputArchive {
public:
    explicit VectorOutputArchive(std::vector<std::uint8_t> &out) : out_(out) {}

    // sizes (count of elements)
    void operator()(std::underlying_type_t<entt::entity> value) {
        writeIntegral(value);
    }

    // entities
    void operator()(entt::entity value) {
        auto v = static_cast<std::underlying_type_t<entt::entity>>(value);
        writeIntegral(v);
    }

    // generic component serialization via memcpy for trivially copyable types
    template <typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>> operator()(const T &value) {
        writeBytes(reinterpret_cast<const std::uint8_t *>(&value), sizeof(T));
    }

    // std::string specialization
    void operator()(const std::string &s) {
        std::uint64_t len = static_cast<std::uint64_t>(s.size());
        writeIntegral(len);
        writeBytes(reinterpret_cast<const std::uint8_t *>(s.data()), s.size());
    }

private:
    template <typename I>
    void writeIntegral(I v) {
        static_assert(std::is_integral_v<I>);
        writeBytes(reinterpret_cast<const std::uint8_t *>(&v), sizeof(I));
    }

    void writeBytes(const std::uint8_t *data, std::size_t len) {
        const auto old = out_.size();
        out_.resize(old + len);
        std::memcpy(out_.data() + old, data, len);
    }

    std::vector<std::uint8_t> &out_;
};

class VectorInputArchive {
public:
    explicit VectorInputArchive(const std::vector<std::uint8_t> &in) : in_(in) {}

    // sizes (count of elements)
    void operator()(std::underlying_type_t<entt::entity> &value) {
        readIntegral(value);
    }

    // entities
    void operator()(entt::entity &value) {
        std::underlying_type_t<entt::entity> tmp{};
        readIntegral(tmp);
        value = static_cast<entt::entity>(tmp);
    }

    // trivially copyable components
    template <typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>> operator()(T &value) {
        readBytes(reinterpret_cast<std::uint8_t *>(&value), sizeof(T));
    }

    // std::string specialization
    void operator()(std::string &s) {
        std::uint64_t len{};
        readIntegral(len);
        s.resize(static_cast<std::size_t>(len));
        if (len) {
            readBytes(reinterpret_cast<std::uint8_t *>(s.data()), static_cast<std::size_t>(len));
        }
    }

private:
    template <typename I>
    void readIntegral(I &v) {
        static_assert(std::is_integral_v<I>);
        readBytes(reinterpret_cast<std::uint8_t *>(&v), sizeof(I));
    }

    void readBytes(std::uint8_t *dst, std::size_t len) {
        if (offset_ + len > in_.size()) {
            throw std::runtime_error("VectorInputArchive: out of data");
        }
        std::memcpy(dst, in_.data() + offset_, len);
        offset_ += len;
    }

    const std::vector<std::uint8_t> &in_;
    std::size_t offset_{};
};
