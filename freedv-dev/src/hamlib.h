#ifndef HAMLIB_H
#define HAMLIB_H

extern "C" {
#include <hamlib/rig.h>
}
#include <wx/combobox.h>
#include <vector>

class Hamlib {

    public:
        Hamlib();
        ~Hamlib();
        void populateComboBox(wxComboBox *cb);
        bool connect(unsigned int rig_index, const char *serial_port);
        bool ptt(bool press);
        void close(void);

        typedef std::vector<const struct rig_caps *> riglist_t;

    private:
        RIG *m_rig;
        /* Sorted list of rigs. */
        riglist_t m_rigList;
};

#endif /*HAMLIB_H*/
