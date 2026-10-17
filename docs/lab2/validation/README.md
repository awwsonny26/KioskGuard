# Перевірки

Build/test/coverage/sanitize логи — реальний stdout/stderr команд. Для читання
CR progress перетворено на LF й прибрано кінцевий whitespace; raw копії
лишились у Lab02/Work/validation. Дані й результати не замінено.
SHA256 файли — прямий shasum результат. Vendor хеші — реальні файли ST/CMSIS.
CI8 log — gh run view --log. Фінальний CI лог зберігається поза Git після push,
щоб останній коміт не вимагав ще одного коміту заради власного журналу.
