#include "Boot/Boot.h"
#include "Log/Log.h"
#include <coroutine>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <optional>

// ============================================================================
// БАЗОВАЯ ИНФРАСТРУКТУРА ДЛЯ КОРУТИН
// ============================================================================

/**
 * Простая корутина-генератор для демонстрации основных концепций корутин.
 * Эта структура представляет корутину, которая может возвращать значения
 * последовательно (как генератор в Python).
 */
template<typename T>
struct Generator {
    // Типы, необходимые для корутин C++20
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    
    /**
     * promise_type - это "контракт" между корутиной и вызывающим кодом.
     * Он определяет, как корутина должна себя вести при создании, 
     * возврате значений и завершении.
     */
    struct promise_type {
        T current_value{};
        
        // Вызывается при создании корутины
        Generator get_return_object() {
            return Generator{handle_type::from_promise(*this)};
        }
        
        // Определяет поведение при первом запуске корутины
        std::suspend_always initial_suspend() { return {}; }
        
        // Определяет поведение при завершении корутины
        std::suspend_always final_suspend() noexcept { return {}; }
        
        // Вызывается при использовании co_yield
        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }
        
        // Обработка исключений в корутине
        void unhandled_exception() {
            // В данной реализации просто завершаем программу
            std::terminate();
        }
        
        // Запрещаем co_return с значением (используем только co_yield)
        void return_void() {}
    };
    
    handle_type coro_handle;
    
    explicit Generator(handle_type h) : coro_handle(h) {}
    
    // Деструктор - освобождаем ресурсы корутины
    ~Generator() {
        if (coro_handle) {
            coro_handle.destroy();
        }
    }
    
    // Запрещаем копирование, разрешаем перемещение
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    Generator(Generator&& other) noexcept 
        : coro_handle(other.coro_handle) {
        other.coro_handle = {};
    }
    
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (coro_handle) {
                coro_handle.destroy();
            }
            coro_handle = other.coro_handle;
            other.coro_handle = {};
        }
        return *this;
    }
    
    // Получить следующее значение от корутины
    std::optional<T> next() {
        if (!coro_handle || coro_handle.done()) {
            return std::nullopt;
        }
        
        coro_handle.resume();
        
        if (coro_handle.done()) {
            return std::nullopt;
        }
        
        return coro_handle.promise().current_value;
    }
    
    // Проверка, завершена ли корутина
    bool done() const {
        return !coro_handle || coro_handle.done();
    }
};

/**
 * Асинхронная задача - корутина для выполнения асинхронных операций.
 * Это более продвинутый пример, показывающий как создавать awaitable объекты.
 */
template<typename T>
struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    
    struct promise_type {
        std::optional<T> result;
        bool has_error = false;
        std::string error_message;
        
        Task get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(T value) {
            result = value;
        }
        
        void unhandled_exception() {
            has_error = true;
            error_message = "Unhandled exception in coroutine";
        }
    };
    
    handle_type coro_handle;
    
    explicit Task(handle_type h) : coro_handle(h) {}
    
    ~Task() {
        if (coro_handle) {
            coro_handle.destroy();
        }
    }
    
    // Запрещаем копирование, разрешаем перемещение
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : coro_handle(other.coro_handle) {
        other.coro_handle = {};
    }
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro_handle) {
                coro_handle.destroy();
            }
            coro_handle = other.coro_handle;
            other.coro_handle = {};
        }
        return *this;
    }
    
    // Получить результат выполнения задачи
    std::optional<T> get_result() {
        // Ждем завершения корутины
        while (!coro_handle.done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Проверяем наличие ошибки
        if (coro_handle.promise().has_error) {
            Log::Info("Ошибка в корутине: " + coro_handle.promise().error_message);
            return std::nullopt;
        }
        
        // Возвращаем результат
        return coro_handle.promise().result;
    }
    
    // Проверка готовности результата
    bool is_ready() const {
        return coro_handle && coro_handle.done();
    }
};

/**
 * Простой awaitable объект для имитации асинхронного ожидания.
 * Показывает, как создавать пользовательские awaitable объекты.
 */
struct SleepAwaitable {
    std::chrono::milliseconds duration;
    
    explicit SleepAwaitable(std::chrono::milliseconds d) : duration(d) {}
    
    // Определяет, нужно ли приостанавливать корутину
    bool await_ready() const noexcept { 
        return duration.count() <= 0; 
    }
    
    // Вызывается когда корутина приостанавливается
    void await_suspend(std::coroutine_handle<> handle) const {
        // Запускаем асинхронное ожидание в отдельном потоке
        std::thread([handle, d = duration]() {
            std::this_thread::sleep_for(d);
            handle.resume();
        }).detach();
    }
    
    // Возвращает результат ожидания (в данном случае void)
    void await_resume() const noexcept {}
};

// Удобная функция для создания SleepAwaitable
SleepAwaitable sleep_for(std::chrono::milliseconds duration) {
    return SleepAwaitable{duration};
}

// ============================================================================
// ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ КОРУТИН
// ============================================================================

/**
 * Генератор чисел Фибоначчи - классический пример корутины-генератора.
 * Демонстрирует использование co_yield для возврата значений.
 */
Generator<int> fibonacci_generator(int count) {
    Log::Info("Запуск генератора чисел Фибоначчи");
    
    if (count <= 0) {
        co_return; // Завершаем корутину без значений
    }
    
    int a = 0, b = 1;
    
    // Первое число Фибоначчи
    co_yield a;
    
    if (count == 1) {
        co_return;
    }
    
    // Второе число Фибоначчи
    co_yield b;
    
    // Генерируем остальные числа
    for (int i = 2; i < count; ++i) {
        int next = a + b;
        a = b;
        b = next;
        co_yield next; // Возвращаем следующее число и приостанавливаем выполнение
    }
    
    Log::Info("Генератор завершил работу");
}

/**
 * Генератор простых чисел с использованием решета Эратосфена.
 * Более сложный пример генератора с вычислениями.
 */
Generator<int> prime_generator(int limit) {
    Log::Info("Запуск генератора простых чисел до " + std::to_string(limit));
    
    if (limit < 2) {
        co_return;
    }
    
    // Создаем решето Эратосфена
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;
    
    for (int i = 2; i <= limit; ++i) {
        if (is_prime[i]) {
            co_yield i; // Возвращаем простое число
            
            // Помечаем все кратные как составные
            for (int j = i * i; j <= limit; j += i) {
                is_prime[j] = false;
            }
        }
    }
    
    Log::Info("Генератор простых чисел завершил работу");
}

/**
 * Асинхронная задача для вычисления факториала.
 * Демонстрирует использование co_await и co_return.
 */
Task<long long> factorial_async(int n) {
    Log::Info("Начало асинхронного вычисления факториала " + std::to_string(n));
    
    if (n < 0) {
        Log::Info("Ошибка: Факториал не определен для отрицательных чисел");
        co_return -1; // Возвращаем код ошибки
    }
    
    long long result = 1;
    
    for (int i = 1; i <= n; ++i) {
        result *= i;
        
        // Имитируем медленные вычисления с периодическими паузами
        if (i % 3 == 0) {
            Log::Info("Промежуточный результат для " + std::to_string(i) + ": " + std::to_string(result));
            co_await sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    Log::Info("Завершение асинхронного вычисления факториала");
    co_return result; // Возвращаем финальный результат
}

/**
 * Корутина, которая выполняет несколько асинхронных операций последовательно.
 * Показывает композицию корутин и использование co_await.
 */
Task<std::string> complex_async_operation(const std::string& input) {
    Log::Info("Начало сложной асинхронной операции с входом: " + input);
    
    // Шаг 1: Имитируем сетевой запрос
    Log::Info("Шаг 1: Выполнение сетевого запроса...");
    co_await sleep_for(std::chrono::milliseconds(200));
    
    std::string step1_result = "processed_" + input;
    Log::Info("Шаг 1 завершен: " + step1_result);
    
    // Шаг 2: Имитируем обработку данных
    Log::Info("Шаг 2: Обработка данных...");
    co_await sleep_for(std::chrono::milliseconds(150));
    
    std::string step2_result = step1_result + "_analyzed";
    Log::Info("Шаг 2 завершен: " + step2_result);
    
    // Шаг 3: Имитируем сохранение результата
    Log::Info("Шаг 3: Сохранение результата...");
    co_await sleep_for(std::chrono::milliseconds(100));
    
    std::string final_result = step2_result + "_saved";
    Log::Info("Операция полностью завершена: " + final_result);
    
    co_return final_result;
}

/**
 * Демонстрация работы с генераторами.
 */
void demonstrate_generators() {
    Log::Info("=== ДЕМОНСТРАЦИЯ ГЕНЕРАТОРОВ ===");
    
    // Пример 1: Числа Фибоначчи
    Log::Info("\n--- Генератор чисел Фибоначчи ---");
    auto fib_gen = fibonacci_generator(10);
    
    std::string fib_sequence = "Последовательность Фибоначчи: ";
    while (auto value = fib_gen.next()) {
        fib_sequence += std::to_string(*value) + " ";
    }
    Log::Info(fib_sequence);
    
    // Пример 2: Простые числа
    Log::Info("\n--- Генератор простых чисел ---");
    auto prime_gen = prime_generator(30);
    
    std::string prime_sequence = "Простые числа до 30: ";
    while (auto prime = prime_gen.next()) {
        prime_sequence += std::to_string(*prime) + " ";
    }
    Log::Info(prime_sequence);
}

/**
 * Демонстрация работы с асинхронными задачами.
 */
void demonstrate_async_tasks() {
    Log::Info("\n=== ДЕМОНСТРАЦИЯ АСИНХРОННЫХ ЗАДАЧ ===");
    
    // Пример 1: Вычисление факториала
    Log::Info("\n--- Асинхронное вычисление факториала ---");
    auto factorial_task = factorial_async(10);
    
    // Ожидаем результат
    auto result_opt = factorial_task.get_result();
    if (result_opt.has_value()) {
        auto result = result_opt.value();
        if (result > 0) {
            Log::Info("Факториал 10 = " + std::to_string(result));
        } else {
            Log::Info("Ошибка при вычислении факториала");
        }
    } else {
        Log::Info("Не удалось получить результат вычисления факториала");
    }
    
    // Пример 2: Сложная асинхронная операция
    Log::Info("\n--- Сложная асинхронная операция ---");
    auto complex_task = complex_async_operation("example_data");
    
    auto complex_result_opt = complex_task.get_result();
    if (complex_result_opt.has_value()) {
        Log::Info("Результат сложной операции: " + complex_result_opt.value());
    } else {
        Log::Info("Ошибка в сложной операции");
    }
}

/**
 * Демонстрация параллельного выполнения задач.
 */
void demonstrate_parallel_tasks() {
    Log::Info("\n=== ДЕМОНСТРАЦИЯ ПАРАЛЛЕЛЬНЫХ ЗАДАЧ ===");
    
    // Запускаем несколько задач параллельно
    auto task1 = factorial_async(5);
    auto task2 = factorial_async(7);
    auto task3 = complex_async_operation("parallel_test");
    
    Log::Info("Запущены 3 параллельные задачи...");
    
    // Ожидаем результаты
    auto start_time = std::chrono::steady_clock::now();
    
    auto result1_opt = task1.get_result();
    auto result2_opt = task2.get_result();
    auto result3_opt = task3.get_result();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (result1_opt.has_value() && result1_opt.value() > 0) {
        Log::Info("Результат задачи 1 (факториал 5): " + std::to_string(result1_opt.value()));
    } else {
        Log::Info("Ошибка в задаче 1");
    }
    
    if (result2_opt.has_value() && result2_opt.value() > 0) {
        Log::Info("Результат задачи 2 (факториал 7): " + std::to_string(result2_opt.value()));
    } else {
        Log::Info("Ошибка в задаче 2");
    }
    
    if (result3_opt.has_value()) {
        Log::Info("Результат задачи 3: " + result3_opt.value());
    } else {
        Log::Info("Ошибка в задаче 3");
    }
    
    Log::Info("Общее время выполнения: " + std::to_string(duration.count()) + " мс");
}

int main(int argc, const char** argv)
{
    Boot::LogHeader(argc, argv);
    Log::Info("=== ДЕМОНСТРАЦИЯ КОРУТИН C++20 ===");
    Log::Info("Этот пример показывает различные аспекты работы с корутинами:");
    Log::Info("- Генераторы (co_yield)");
    Log::Info("- Асинхронные задачи (co_await, co_return)");
    Log::Info("- Пользовательские awaitable объекты");
    Log::Info("- Параллельное выполнение корутин");
    
    // Демонстрируем генераторы
    demonstrate_generators();

//#ifndef __EMSCRIPTEN__
    // Демонстрируем асинхронные задачи
    demonstrate_async_tasks();
    
    // Демонстрируем параллельное выполнение
    demonstrate_parallel_tasks();
//#endif

    Log::Info("\n=== ВСЕ ДЕМОНСТРАЦИИ ЗАВЕРШЕНЫ УСПЕШНО ===");
    
    return 0;
}
