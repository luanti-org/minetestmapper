#pragma once

#include <list>
#include <string>

struct Player
{
	std::string name;
	float x, y, z;
};

class PlayerAttributes
{
public:
	typedef std::list<Player> Players;

	PlayerAttributes(const std::string &worldDir);
	Players::const_iterator begin() const;
	Players::const_iterator end() const;

private:
	Players m_players;
};
