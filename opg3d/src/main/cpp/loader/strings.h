// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// All native status copy is centralized here; HTML copy is localized in
// ui.html.
namespace launcher::text {
constexpr const char *searching = "Проверка сохранённых и локальных серверов…";
constexpr const char *ready = "Выберите совместимый сервер и нажмите «Играть».";
constexpr const char *invalid = "Введите адрес сервера и порт от 1 до 65535.";
constexpr const char *unavailable =
    "Сервер недоступен. Проверьте адрес, порт и подключение.";
constexpr const char *incompatible = "Версия сервера не подходит для клиента "
                                     "23.1.3, либо сервер на обслуживании.";
constexpr const char *checking = "Проверка версии и подписи обновления…";
constexpr const char *signature =
    "Подпись обновления недействительна. Запуск заблокирован.";
constexpr const char *downloading = "Загрузка обновления фронтэнда…";
constexpr const char *damaged =
    "Обновление не прошло проверку. Предыдущий кэш сохранён.";
constexpr const char *cache = "Используем проверенную библиотеку из кэша…";
constexpr const char *loading = "Запуск игрового модуля…";
constexpr const char *loadfail =
    "Не удалось загрузить игровой модуль. Предыдущий кэш сохранён.";
constexpr const char *storage =
    "Не удалось сохранить обновление в приватном кэше.";
constexpr const char *downgrade =
    "Сервер предлагает устаревшее обновление. Запуск заблокирован.";
constexpr const char *bootfail = "Предыдущий запуск модуля завершился "
                                 "неудачно. Нужна новая версия на сервере.";
constexpr const char *started = "Запускаем игру…";
} // namespace launcher::text
