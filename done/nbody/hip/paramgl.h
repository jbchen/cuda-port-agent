// HIP compatibility replacement for NVIDIA's paramgl.h
// Provides a simple on-screen parameter slider UI.

#ifndef PARAMGL_H
#define PARAMGL_H

#include <cstdio>
#include <cstring>
#include <vector>

#if defined(__APPLE__) || defined(MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif

template <typename T> class Param
{
public:
    Param(const char *name, T val, T min, T max, T step, T *ptr)
        : m_name(name)
        , m_val(val)
        , m_min(min)
        , m_max(max)
        , m_step(step)
        , m_ptr(ptr)
    {
        *m_ptr = val;
    }

    T    getValue() const { return m_val; }
    void setValue(T val)
    {
        m_val = val;
        if (m_val < m_min) m_val = m_min;
        if (m_val > m_max) m_val = m_max;
        *m_ptr = m_val;
    }

    void increment() { setValue(m_val + m_step); }
    void decrement() { setValue(m_val - m_step); }

    const char *getName() const { return m_name; }
    T           getMin() const { return m_min; }
    T           getMax() const { return m_max; }

private:
    const char *m_name;
    T           m_val, m_min, m_max, m_step;
    T          *m_ptr;
};

class ParamListGL
{
public:
    ParamListGL(const char *name = "")
        : m_name(name)
        , m_barColorR(0.8f)
        , m_barColorG(0.8f)
        , m_barColorB(0.0f)
        , m_active(-1)
        , m_start_x(0)
        , m_start_y(0)
        , m_bar_x(250)
        , m_bar_w(250)
        , m_bar_h(15)
        , m_bar_offset(20)
        , m_text_x(5)
    {
    }

    ~ParamListGL()
    {
        for (auto p : m_params) delete p;
    }

    void SetBarColorInner(float r, float g, float b)
    {
        m_barColorR = r;
        m_barColorG = g;
        m_barColorB = b;
    }

    void AddParam(Param<float> *p) { m_params.push_back(p); }

    void Render(int x, int y)
    {
        m_start_x = x;
        m_start_y = y;

        for (int i = 0; i < (int)m_params.size(); i++) {
            int bar_y = m_start_y + i * m_bar_offset + 15;

            // draw label
            glColor3f(1.0f, 1.0f, 1.0f);
            glRasterPos2f((float)(m_start_x + m_text_x), (float)(bar_y + 11));
            const char *name = m_params[i]->getName();
            while (*name) {
                glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *name);
                name++;
            }

            // draw bar background
            float x0 = (float)(m_start_x + m_bar_x);
            float x1 = x0 + m_bar_w;
            float y0 = (float)bar_y;
            float y1 = y0 + m_bar_h;

            glColor3f(0.25f, 0.25f, 0.25f);
            glBegin(GL_QUADS);
            glVertex2f(x0, y0);
            glVertex2f(x1, y0);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
            glEnd();

            // draw filled portion
            float t     = (m_params[i]->getValue() - m_params[i]->getMin()) / (m_params[i]->getMax() - m_params[i]->getMin());
            float bar_w = t * m_bar_w;
            glColor3f(m_barColorR, m_barColorG, m_barColorB);
            glBegin(GL_QUADS);
            glVertex2f(x0, y0);
            glVertex2f(x0 + bar_w, y0);
            glVertex2f(x0 + bar_w, y1);
            glVertex2f(x0, y1);
            glEnd();
        }
    }

    bool Mouse(int x, int y, int button, int state)
    {
        if (button == 0 && state == 0) { // GLUT_LEFT_BUTTON, GLUT_DOWN
            for (int i = 0; i < (int)m_params.size(); i++) {
                int bar_y = m_start_y + i * m_bar_offset + 15;
                if (x >= m_start_x + m_bar_x && x <= m_start_x + m_bar_x + m_bar_w && y >= bar_y
                    && y <= bar_y + m_bar_h) {
                    m_active = i;
                    Motion(x, y);
                    return true;
                }
            }
        }
        else {
            m_active = -1;
        }
        return false;
    }

    bool Motion(int x, int /*y*/)
    {
        if (m_active >= 0 && m_active < (int)m_params.size()) {
            float t = ((float)(x - m_start_x - m_bar_x)) / (float)m_bar_w;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float val = m_params[m_active]->getMin() + t * (m_params[m_active]->getMax() - m_params[m_active]->getMin());
            m_params[m_active]->setValue(val);
            return true;
        }
        return false;
    }

    void Special(int /*key*/, int /*x*/, int /*y*/) {}

private:
    const char               *m_name;
    std::vector<Param<float>*> m_params;
    float                      m_barColorR, m_barColorG, m_barColorB;
    int                        m_active;
    int                        m_start_x, m_start_y;
    int                        m_bar_x, m_bar_w, m_bar_h, m_bar_offset, m_text_x;
};

#endif // PARAMGL_H
