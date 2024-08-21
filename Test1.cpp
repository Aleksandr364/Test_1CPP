#include <iostream>
#include <vector>

// GRAY "grayscale" 0-255 1 байт (оттенки серого цвета).
// RGB "red green blue" 3 байта.
// BGR "blue green red" 3 байта (обратный порядок RGB).
// RGBA "red green blue alpha" 4 байта ("альфа-канал" - уровень прозрачности).
struct image
{
    int width, height;
    int format; // 0=GRAY 1=RGB 2=BGR
    uint8_t* data;
};
struct box
{
    int x1, y1; 
    int x2, y2;
    int type; // 0=FACE 1=GUN 2=MASK
};
struct frame
{
    image img;
    std::vector<box> boxes;
};

// Класс для работы над объединением боксов.
//******************************************
class BoxClass
{
public:

    enum class boxtype { face, gun, mask, null };

protected:

    boxtype type = boxtype::null;
    std::vector<box> boxes;

protected:

    bool CheckBoxValid(const box& Box) { return (Box.x1 < Box.x2 && Box.y1 < Box.y2); } // Бокс действительно прямоугольный.

    // Создаём вектор с тремя классами BoxClass, каждому из коротых задаём конкретный тип (type), и заполняем векторы боксов согласно заданному типу.
    std::vector<BoxClass> VectOfBoxClasses(const frame& Frame, const frame& Frame2 = {})
    {
        std::vector<BoxClass> vectOfBoxClasses;
        vectOfBoxClasses.push_back(BoxClass(boxtype::face));
        vectOfBoxClasses.push_back(BoxClass(boxtype::gun));
        vectOfBoxClasses.push_back(BoxClass(boxtype::mask));

        // Сортируем боксы кадра Frame.
        if (!Frame.boxes.empty())
        {
            for (size_t i = 0; i < Frame.boxes.size(); i++)
            {
                if (Frame.boxes[i].type < 0 || Frame.boxes[i].type > 2 || !CheckBoxValid(Frame.boxes[i])) continue;

                vectOfBoxClasses[Frame.boxes[i].type].push_back(Frame.boxes[i]);
            }
        }
        if (Frame2.boxes.empty()) return vectOfBoxClasses;
        // Сортируем боксы кадра Frame2.
        for (size_t i = 0; i < Frame2.boxes.size(); i++)
        {
            if (Frame2.boxes[i].type < 0 || Frame2.boxes[i].type > 2 || !CheckBoxValid(Frame2.boxes[i])) continue;

            vectOfBoxClasses[Frame2.boxes[i].type].push_back(Frame2.boxes[i]);
        }

        return vectOfBoxClasses;
    }

    bool compare2Boxes(const box& Box1, const box& Box2, const float threshold)
    {
        // Найдём координаты предполагаемого пересечения.
        int x1_ = std::max(Box1.x1, Box2.x1);
        int y1_ = std::max(Box1.y1, Box2.y1);
        int x2_ = std::min(Box1.x2, Box2.x2);
        int y2_ = std::min(Box1.y2, Box2.y2);
        // Убедимся, что пересечение существует.
        if (x1_ >= x2_ || y1_ >= y2_) return false;
        // Найдём площадь пересечения.
        int intersection = (x2_ - x1_) * (y2_ - y1_);
        // Найдём площадь объединения.
        int combination = (Box1.x2 - Box1.x1) * (Box1.y2 - Box1.y1) + (Box2.x2 - Box2.x1) * (Box2.y2 - Box2.y1) - intersection;
        // Найдём коэффициент IOU.
        float IOU = float(intersection) / float(combination);
        return (IOU >= threshold);
    }

    box combine2Boxes(const box& Box1, const box& Box2)
    {
        // Найдём координаты для результирующего newBox.
        int x1_ = std::min(Box1.x1, Box2.x1); // Найдём крайнюю левую координату.
        int y1_ = std::min(Box1.y1, Box2.y1); // Найдём самую нижнюю координату.
        int x2_ = std::max(Box1.x2, Box2.x2); // Найдём крайнюю правую координату.
        int y2_ = std::max(Box1.y2, Box2.y2); // Найдём самую верхнюю координату.
        // Результирующий бокс.
        box newBox;
        newBox.type = Box1.type;
        newBox.x1 = x1_;
        newBox.x2 = x2_;
        newBox.y1 = y1_;
        newBox.y2 = y2_;
        return newBox;
    }

public:

    BoxClass(boxtype type) : type(type) {}

    void push_back(const box& Box) { boxes.push_back(Box); }

    // Объединение боксов.
    void cleanBoxes(frame& Frame, const float threshold)
    {
        if (threshold <= 0 || threshold > 1) throw std::invalid_argument("invalid threshold value");

        std::vector<BoxClass> boxClasses = VectOfBoxClasses(Frame);

        for (auto& currentBoxClass:boxClasses)
        {
            if (currentBoxClass.boxes.empty()) continue;
            for (size_t j = 0; j < currentBoxClass.boxes.size() - 1; j++)
            {
                for (size_t z = j + 1; z < currentBoxClass.boxes.size(); z++)
                {
                    if (compare2Boxes(currentBoxClass.boxes[j], currentBoxClass.boxes[z], threshold))
                    {
                        currentBoxClass.boxes[j] = combine2Boxes(currentBoxClass.boxes[j], currentBoxClass.boxes[z]);
                        currentBoxClass.boxes.erase(currentBoxClass.boxes.begin() + z);
                        z -= 1;
                    }
                }
            }
        }
        // Очищаем вектор boxes рамки Frame.
        Frame.boxes.clear();
        // Заполняем очищенный вектор новыми боксами.
        for (size_t i = 0; i < boxClasses.size(); i++)
        {
            for (size_t j = 0; j < boxClasses[i].boxes.size(); j++)
            {
                Frame.boxes.push_back(boxClasses[i].boxes[j]);
            }
        }
    }

    // Объединение фреймов.
    frame combine2frames(const frame& f1, const frame& f2, const float threshold)
    {
        if (threshold <= 0 || threshold > 1) throw std::invalid_argument("invalid threshold value");
        
        // Результирующий кадр.
        frame resultFrame;
        resultFrame.img = f1.img;
        
        std::vector<BoxClass> boxClasses = VectOfBoxClasses(f1, f2);

        // Очищаем от лишних боксов.
        for (auto& currentBoxClass : boxClasses)
        {
            if (currentBoxClass.boxes.empty()) continue;
            for (size_t j = 0; j < currentBoxClass.boxes.size() - 1; j++)
            {
                for (size_t z = j + 1; z < currentBoxClass.boxes.size(); z++)
                {
                    if (compare2Boxes(currentBoxClass.boxes[j], currentBoxClass.boxes[z], threshold))
                    {
                        currentBoxClass.boxes[j] = combine2Boxes(currentBoxClass.boxes[j], currentBoxClass.boxes[z]);
                        currentBoxClass.boxes.erase(currentBoxClass.boxes.begin() + z);
                        z -= 1;
                    }
                }
            }
        }
        // Заполняем вектор боксов результирующего кадра.
        for (size_t i = 0; i < boxClasses.size(); i++)
        {
            for (size_t j = 0; j < boxClasses[i].boxes.size(); j++)
            {
                resultFrame.boxes.push_back(boxClasses[i].boxes[j]);
            }
        }

        return resultFrame;
    }

    // Тесты.
    // *******
    // Тест функции TestVectOfBoxClasses().
    std::vector<BoxClass> TestVectOfBoxClasses(const frame& Frame) { return VectOfBoxClasses(Frame); }
};

// Функция преобразует формат изображения из RGB в BGR.
bool rgb2bgr(image& img)
{
    if (!img.data || img.format != 1) return false;
    
    uint8_t* curData = img.data;
    unsigned int pixels = img.width * img.height;

    for (unsigned int i = 0; i < pixels; i++)
    {
        // std::swap(curData[0], curData[2]);
        uint8_t temp = curData[0];
        curData[0] = curData[2];
        curData[2] = temp;
        curData += 3;
    }
    return true;
}
// Функция очищает кадр, оставляя одну рамку для общих объектов.
// Объект считается общим для двух box, если их IOU >= threshold. 
void frame_clean(frame& f, const float threshold)
{
    BoxClass boxClass(BoxClass::boxtype::null);
    boxClass.cleanBoxes(f, threshold);
}
// Функция объединяет обнаруженные объекты из двух кадров в один.
// Объект считается общим для двух box, если их IOU >= threshold.
// Гарантируется, что f1.img == f2.img.
frame union_frames(frame& f1, frame& f2, const float threshold)
{
    BoxClass boxClass(BoxClass::boxtype::null);
    return boxClass.combine2frames(f1, f2, threshold);
}

// Тесты.
//********
// Тестовое изображение.
image CreateTestImage()
{
    uint8_t localData[] = { 1, 2, 3, 1, 2, 3, 1, 2, 3, 4, 5, 6, 4, 5, 6, 4, 5, 6 };
    image testImg;
    testImg.width = 3;
    testImg.height = 2;
    testImg.format = 1;
    unsigned int size = testImg.width * testImg.height * 3;
    testImg.data = new uint8_t[size]{};
    memcpy_s(testImg.data, size, localData, size);
    return testImg;
}
// Тестовая рамка #1.
frame CreateTestFrame()
{
    frame testFrame;
    testFrame.img = CreateTestImage();
    // Проверка краевых условий (полное совпадение, несовпадение, неправильный boxes).
    testFrame.boxes = { {1, 1, 3, 3, 0}, {2, 2, 4, 4, 1}, {0, 0, 0, 0, 0}, {2, 2, 4, 4, 10}, {5, 5, 10, 10, 2}, {5, 5, 10, 10, 2}, {2, 2, 4, 4, -5}, {2, 3, 6, 7, 0}, {0, 0, 0, 0, 0}, {11, 12, 20, 25, 1}, {2, 3, 6, 7, 0}, {33, 44, 100, 100, 2}, {5, 5, 10, 10, 2}, {2, 3, 6, 7, 100}, {2, 3, 6, 7, 0}, {11, 12, 20, 25, 1}, {2, 3, 6, 7, 0}, {33, 44, 100, 100, -50}, {2, 2, 2, 2, 1}, {5, 5, 10, 10, 2}, {24, 4, 4, 4, -5}, {0, 0, 0, 0, 0} };
    // Проверка рабочих условий #1.
    // testFrame.boxes = { {1, 1, 10, 10, 0}, {5, 5, 15, 15, 0}, {1, 1, 20, 20, 1}, {5, 5, 25, 25, 1}, {1, 1, 12, 12, 2}, {2, 2, 13, 13, 2} };
    // Проверка рабочих условий #2.
    // testFrame.boxes = { {1, 5, 10, 15, 0}, {5, 1, 15, 10, 0}, {2, 1, 11, 10, 1 }, {1, 2, 10, 11, 1} };
    // Проверка рабочих условий #3.
    // testFrame.boxes = { {1, 1, 10, 15, 0}, {2, 2, 9, 16, 0}, {2, 14, 9, 16, 1 }, {1, 1, 10, 15, 1} };
    return testFrame;
}
// Тестовая рамка #2.
frame CreateTestFrame2()
{
    frame testFrame;
    testFrame.img = CreateTestImage();
    // Проверка краевых условий (полное совпадение, несовпадение, неправильный boxes).
    testFrame.boxes = { {1, 1, 3, 3, 0}, {2, 2, 4, 4, 1}, {0, 0, 0, 0, 0}, {2, 2, 4, 4, 10}, {5, 5, 10, 10, 2}, {5, 5, 10, 10, 2}, {2, 2, 4, 4, -5}, {2, 3, 6, 7, 0}, {0, 0, 0, 0, 0}, {11, 12, 20, 25, 1}, {2, 3, 6, 7, 0}, {33, 44, 100, 100, 2}, {5, 5, 10, 10, 2}, {2, 3, 6, 7, 100}, {2, 3, 6, 7, 0}, {11, 12, 20, 25, 1}, {2, 3, 6, 7, 0}, {33, 44, 100, 100, -50}, {2, 2, 2, 2, 1}, {5, 5, 10, 10, 2}, {24, 4, 4, 4, -5}, {0, 0, 0, 0, 0} };
    // Проверка рабочих условий #1.
    // testFrame.boxes = { {1, 1, 10, 10, 0}, {5, 5, 15, 15, 0}, {1, 1, 20, 20, 1}, {5, 5, 25, 25, 1}, {1, 1, 12, 12, 2}, {2, 2, 13, 13, 2} };
    // Проверка рабочих условий #2.
    // testFrame.boxes = { {1, 5, 10, 15, 0}, {5, 1, 15, 10, 0}, {2, 1, 11, 10, 1 }, {1, 2, 10, 11, 1} };
    // Проверка рабочих условий #3.
    // testFrame.boxes = { {1, 1, 10, 15, 0}, {2, 2, 9, 16, 0}, {2, 14, 9, 16, 1 }, {1, 1, 10, 15, 1} };
    return testFrame;
}
// Тест функции rgb2bgr().
void TestRGB2BGR()
{
    rgb2bgr((image&)CreateTestImage());
}
// Тест функции frame_clean().
void TestFrame_Clean()
{
    frame newFrame = CreateTestFrame();
    float threshold = 0.5;
    frame_clean(newFrame, threshold);
}
// Тест функции union_frames().
void TestUnion_frames()
{
    frame testFrame1 = CreateTestFrame();
    frame testFrame2 = CreateTestFrame2();
    float threshold = 0.5;
    frame testFrame3 = union_frames(testFrame1, testFrame2, threshold);
}

int main()
{
    return 0;
}