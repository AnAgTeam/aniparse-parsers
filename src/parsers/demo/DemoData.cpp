/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/demo/DemoData.hpp"

namespace aniparse::parsers::demo {

std::string asset_url(std::string_view key) {
	std::string url(scheme);
	url.append(key);
	return url;
}

namespace {

std::vector<DemoChapter> arc(int count, std::initializer_list<const char*> named) {
	std::vector<DemoChapter> out;
	auto it = named.begin();
	for (int i = 1; i <= count; ++i) {
		std::string title = (it != named.end()) ? *it++ : std::string{};
		out.push_back(DemoChapter{ .number = i, .title = std::move(title) });
	}
	return out;
}

/// The four drawn covers, cycled so the catalogue can hold more titles than it has
/// artwork — plenty for a browse grid that actually scrolls and pages.
const char* cover_cycle(int id) {
	static const char* covers[] = {
	    "covers/fox-lantern.png", "covers/midnight-tram.png",
	    "covers/ink-snow.png",    "covers/red-umbrella.png",
	};
	return covers[(id - 1) % 4];
}

/// Compact builder for the many secondary titles (no per-title related list).
DemoTitle mk(int id, const char* slug, const char* title, const char* original,
             double rating, int year, bool ongoing, std::vector<std::string> tags,
             int chapters, const char* description,
             std::vector<std::string> characters = {}) {
	return DemoTitle{
	    .id             = id,
	    .slug           = slug,
	    .title          = title,
	    .original_title = original,
	    .description    = description,
	    .rating         = rating,
	    .year           = year,
	    .ongoing        = ongoing,
	    .author         = "Асагао",
	    .artist         = "Асагао",
	    .cover_key      = cover_cycle(id),
	    .tags           = std::move(tags),
	    .characters     = std::move(characters),
	    .chapters       = arc(chapters, {}),
	    .related        = {},
	};
}

const std::vector<DemoTitle> catalog_titles = [] {
	std::vector<DemoTitle> c;

	c.push_back(DemoTitle{
	    .id             = 1,
	    .slug           = "fox-lantern",
	    .title          = "Лисий фонарь",
	    .original_title = "狐火の提灯",
	    .description    =
	        "У старого моста на окраине города по ночам зажигается фонарь, которого "
	        "нет на картах. Тех, кто идёт на его свет, наутро не находят. Юки берётся "
	        "выяснить, кто держит огонь — и понимает, что фонарь узнаёт её.",
	    .rating         = 9.2,
	    .year           = 2019,
	    .ongoing        = true,
	    .author         = "Асагао",
	    .artist         = "Асагао",
	    .cover_key      = "covers/fox-lantern.png",
	    .tags           = { "мистика", "сверхъестественное", "детектив", "драма", "сёдзё" },
	    .characters     = { "Юки", "Фонарщик", "Каэдэ" },
	    .chapters       = arc(24, { "Охота на лисий огонь", "Мост под дождём", "Тень у воды",
	                                "Голос за спиной" }),
	    .related        = { { "Адаптация", "Лисий фонарь: аниме" },
	                        { "Спин-офф", "Записки фонарщика" },
	                        { "Тот же автор", "Тушь и снег" } },
	});

	c.push_back(DemoTitle{
	    .id             = 2,
	    .slug           = "midnight-tram",
	    .title          = "Полночный трамвай",
	    .original_title = "夜行電車",
	    .description    =
	        "Последний трамвай отходит в 0:00 и едет по маршруту, которого нет в "
	        "расписании. В вагоне — те, кому есть что оставить в этом городе.",
	    .rating         = 8.6,
	    .year           = 2021,
	    .ongoing        = true,
	    .author         = "Асагао",
	    .artist         = "Кицунэ",
	    .cover_key      = "covers/midnight-tram.png",
	    .tags           = { "мистика", "повседневность", "драма", "сэйнэн" },
	    .characters     = { "Кондуктор", "Рэн" },
	    .chapters       = arc(12, { "Билет в один конец", "Депо" }),
	    .related        = { { "Тот же автор", "Лисий фонарь" } },
	});

	c.push_back(DemoTitle{
	    .id             = 3,
	    .slug           = "ink-snow",
	    .title          = "Тушь и снег",
	    .original_title = "墨と雪",
	    .description    =
	        "Молодая художница уезжает в горную деревню, где снег не тает даже летом, "
	        "а каждая нарисованная тушью картина сбывается — не так, как задумано.",
	    .rating         = 8.9,
	    .year           = 2018,
	    .ongoing        = false,
	    .author         = "Асагао",
	    .artist         = "Асагао",
	    .cover_key      = "covers/ink-snow.png",
	    .tags           = { "драма", "исторический", "сверхъестественное", "сёдзё" },
	    .characters     = { "Сумидзу", "Юки" },
	    .chapters       = arc(9, { "Первый снег", "Кисть" }),
	    .related        = { { "Тот же автор", "Лисий фонарь" } },
	});

	c.push_back(DemoTitle{
	    .id             = 4,
	    .slug           = "red-umbrella",
	    .title          = "Квартал красных зонтов",
	    .original_title = "紅傘の街",
	    .description    =
	        "В квартале, где всегда идёт дождь, зонт цвета крови передаётся из рук в "
	        "руки — и каждый новый владелец платит за укрытие свою цену.",
	    .rating         = 8.1,
	    .year           = 2022,
	    .ongoing        = true,
	    .author         = "Хигана",
	    .artist         = "Хигана",
	    .cover_key      = "covers/red-umbrella.png",
	    .tags           = { "триллер", "мистика", "детектив", "сэйнэн" },
	    .characters     = { "Инспектор Амэ", "Хигана" },
	    .chapters       = arc(15, { "Дождь не кончается", "Красный зонт" }),
	    .related        = {},
	});

	// Secondary titles — reuse the four covers, fill the grid, make browsing page.
	c.push_back(mk(5,  "paper-crane",   "Бумажный журавль",     "折鶴", 7.8, 2020, true,
	               { "драма", "школа", "романтика" }, 8,
	               "Каждый сложенный журавль сбывается — но только один из тысячи."));
	c.push_back(mk(6,  "salt-garden",   "Соляной сад",          "塩の庭", 8.4, 2017, false,
	               { "фэнтези", "приключения", "сэйнэн" }, 21,
	               "За солёной стеной растёт сад, откуда не возвращаются прежними."));
	c.push_back(mk(7,  "ninth-bell",    "Девятый колокол",      "九の鐘", 8.0, 2023, true,
	               { "мистика", "детектив", "триллер" }, 6,
	               "Колокол бьёт девять раз только когда в городе исчезает человек.",
	               { "Кэй", "Звонарь" }));
	c.push_back(mk(8,  "glass-carp",    "Стеклянный карп",      "硝子の鯉", 7.5, 2019, true,
	               { "повседневность", "драма" }, 11,
	               "Мальчик ловит в канале рыбу, которой не должно существовать."));
	c.push_back(mk(9,  "ash-festival",  "Праздник пепла",       "灰祭", 8.7, 2016, false,
	               { "исторический", "драма", "мистика" }, 19,
	               "Раз в сто лет деревня сжигает то, что копила весь век."));
	c.push_back(mk(10, "north-window",  "Северное окно",        "北窓", 7.9, 2022, true,
	               { "романтика", "школа", "сёдзё" }, 9,
	               "В окне общежития по ночам виден город, которого нет днём."));
	c.push_back(mk(11, "iron-koi",      "Железный сом",         "鉄鯰", 8.2, 2021, true,
	               { "фантастика", "приключения", "сэйнэн" }, 14,
	               "Под затонувшим заводом что-то дышит — и оно железное."));
	c.push_back(mk(12, "moth-lantern",  "Мотыльковый фонарь",   "蛾灯", 8.5, 2018, false,
	               { "мистика", "сверхъестественное", "драма" }, 17,
	               "На свет этого фонаря слетаются не мотыльки, а чужие сны.",
	               { "Га", "Юки" }));
	c.push_back(mk(13, "rain-checker",  "Проверяющий дождь",    "雨検", 7.6, 2024, true,
	               { "триллер", "детектив" }, 4,
	               "Инспектор ходит по кварталу и отмечает, где дождь идёт неправильно."));
	c.push_back(mk(14, "second-shadow", "Вторая тень",          "二影", 8.3, 2020, true,
	               { "мистика", "психология", "сэйнэн" }, 13,
	               "У неё две тени, и вторая иногда уходит по своим делам.",
	               { "Ко", "Вторая тень" }));

	return c;
}();

const std::vector<std::string> page_keys = {
    "ch1/p01.png", "ch1/p02.png", "ch1/p03.png", "ch1/p04.png",
    "ch1/p05.png", "ch1/p06.png", "ch1/p07.png", "ch1/p08.png",
};

} // namespace

const std::vector<DemoTitle>& catalog() { return catalog_titles; }

const DemoTitle* find_by_id(int id) {
	for (const auto& t : catalog_titles) {
		if (t.id == id) {
			return &t;
		}
	}
	return nullptr;
}

const DemoTitle* find_by_slug(std::string_view slug) {
	for (const auto& t : catalog_titles) {
		if (t.slug == slug) {
			return &t;
		}
	}
	return nullptr;
}

const std::vector<std::string>& chapter_page_keys() { return page_keys; }

} // namespace aniparse::parsers::demo
