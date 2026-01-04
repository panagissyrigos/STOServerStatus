#pragma once
#include <string>

enum class ServerState
{
  Online,
  Maintenance,
  Unknown
};

struct StatusResult
{
  ServerState state = ServerState::Unknown;
  std::wstring detail; // short human-readable description
};

class StatusChecker
{
public:
  // URL to poll (change this to whatever stostatus uses).
  // If you know the exact endpoint, set it here and you’re done.
  explicit StatusChecker(std::wstring statusUrl);

  StatusResult CheckOnce() const;

  const std::wstring &StatusUrl() const { return statusUrl_; }

private:
  std::wstring statusUrl_;

  // Heuristic parser: keep it simple and robust.
  static StatusResult InterpretBody(const std::string &body);
};
