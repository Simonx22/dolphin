// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common::AES
{
enum class Mode
{
  Decrypt,
  Encrypt,
};
std::vector<u8> DecryptEncrypt(const u8* key, u8* iv, const u8* src, size_t size, Mode mode);

// Convenience functions
std::vector<u8> Decrypt(const u8* key, u8* iv, const u8* src, size_t size);
std::vector<u8> Encrypt(const u8* key, u8* iv, const u8* src, size_t size);
}  // namespace Common::AES
