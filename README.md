# Himalaya CLI для Nod

Нативный CLI-адаптер для чтения одного IMAP-ящика через официальный Himalaya v2.2.1. Выпуск содержит сборки для Linux ARM64 и AMD64; выбирайте вариант по архитектуре Linux workspace Nod, а не MacBook. Исходники и опубликованные файлы не содержат почтовых данных и настоящих секретов.

## Команды

- `check` — проверяет подключение, прочитав не более одного заголовка из INBOX.
- `mailboxes` — список папок.
- `envelopes [папка] [страница] [размер]` — заголовки писем, максимум 100 за вызов.
- `message <числовой-ID> [папка]` — читает письмо без запроса на установку флага «прочитано».

Другие операции, включая отправку, удаление, перемещение и изменение флагов, адаптер не предоставляет. SMTP не настраивается.

## Подключение в Nod

В CLI access path укажите источник `https://github.com/Absolut108/himalaya-nod-connector/releases/download/v0.1.0/<архив-для-архитектуры>` и SHA-256 из таблицы ниже. Имя команды: `himalaya-nod-connector`; authentication: `env`.

| Linux workspace Nod | Архив | SHA-256 |
| --- | --- | --- |
| ARM64 (`aarch64`) | [himalaya-nod-connector-v0.1.0-linux-aarch64.tar.gz](https://github.com/Absolut108/himalaya-nod-connector/releases/download/v0.1.0/himalaya-nod-connector-v0.1.0-linux-aarch64.tar.gz) | `fe11fcfa7cac5ed36e3d7a65835bff76f1d09da66fa7b7aea9b8a0ea5f188d82` |
| AMD64 (`x86_64`) | [himalaya-nod-connector-v0.1.0-linux-x86_64.tar.gz](https://github.com/Absolut108/himalaya-nod-connector/releases/download/v0.1.0/himalaya-nod-connector-v0.1.0-linux-x86_64.tar.gz) | `9879b58247644ecdd854e9251f5148e8d0f394941a2986a222249133756739a4` |

Для Nod CLI connection задайте только эти environment fields; подписи — для защищённой формы:

```json
{
  "HIMALAYA_EMAIL_ID": "Email address",
  "HIMALAYA_USERNAME_ID": "IMAP username",
  "HIMALAYA_PASSWORD": "IMAP app password",
  "HIMALAYA_IMAP_URL_ID": "IMAPS server URL"
}
```

Имена намеренно заканчиваются на `_ID` или `_PASSWORD`: эти окончания указаны в обоих актуальных разделах Nod, хотя список допустимых суффиксов в документации CLI различается. Значений паролей в `app.json`, CLI-аргументах или Git нет. Для Яндекс Почты укажите сервер `imaps://imap.yandex.ru:993`; используйте пароль приложения с включённым IMAP-доступом. Nod выполняет `check` при подключении. Успешная сборка и синтетические проверки не подтверждают реальное чтение почты: завершите подключение только после проверки писем в Nod.

## Проверка загрузки

На Linux/macOS после скачивания соответствующего архива:

```sh
echo 'fe11fcfa7cac5ed36e3d7a65835bff76f1d09da66fa7b7aea9b8a0ea5f188d82  himalaya-nod-connector-v0.1.0-linux-aarch64.tar.gz' | shasum -a 256 -c
echo '9879b58247644ecdd854e9251f5148e8d0f394941a2986a222249133756739a4  himalaya-nod-connector-v0.1.0-linux-x86_64.tar.gz' | shasum -a 256 -c
```

Запустите строку для скачанного файла. Постоянные прямые ссылки выше ведут на файлы выпуска `v0.1.0`.

## Секреты и временные файлы

Пароль передаётся только в Nod-provided environment и записывается Himalaya в краткоживущую TOML-конфигурацию во временном каталоге с правами `0700`; файл имеет права `0600`. Пароль не передаётся в аргументах процесса. После штатного завершения конфигурация удаляется. Если процесс аварийно завершится принудительным сигналом или системой, ОС может не выполнить очистку.

Поддерживается один IMAP-ящик на подключение. Настройка других IMAP-провайдеров возможна через соответствующие логин и IMAPS URL, но проверка Mail.ru и нескольких аккаунтов не проводилась.

## Сборка из исходников

Официальные архивы Himalaya v2.2.1:

- ARM64: `himalaya.aarch64-linux.tgz`, SHA-256 `1dc21c3dd6d948929e22e5cae49b9d0b3b30ff88fafd4493fd4460c6252e9d90`.
- AMD64: `himalaya.x86_64-linux.tgz`, SHA-256 `5c5ba2724c162f82d0a0c71b6c03224ed44f8bef7b14ace5da0635d3af2665a0`.

Обёртка собирается Zig для нужной musl-архитектуры: `zig cc -target aarch64-linux-musl -O2 -static -Wall -Wextra -Werror connector.c -o himalaya-nod-connector` либо с целью `x86_64-linux-musl`. В архиве должны находиться исполняемые `himalaya` и `himalaya-nod-connector` в корне.

Himalaya распространяется по Apache-2.0: https://github.com/pimalaya/himalaya/blob/v2.2.1/LICENSE-APACHE
