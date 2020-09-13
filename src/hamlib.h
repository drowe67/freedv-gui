#ifndef HAMLIB_H
#define HAMLIB_H

#include <wx/stattext.h>
#include <wx/combobox.h>
#include <vector>
extern "C" {
#include <hamlib/rig.h>
}

class Hamlib {

    public:
        Hamlib();
        ~Hamlib();
        void populateComboBox(wxComboBox *cb);
        bool connect(unsigned int rig_index, const char *serial_port, const int serial_rate, const int civ_hex = 0);
        bool ptt(bool press, wxString &hamlibError);
        void enable_sideband_detection(wxStaticText* statusBox);
        void disable_sideband_detection();
        void close(void);
        int get_serial_rate(void);
        int get_data_bits(void);
        int get_stop_bits(void);

        typedef std::vector<const struct rig_caps *> riglist_t;

    private:
        static int hamlib_freq_cb(RIG* rig, vfo_t currVFO, freq_t currFreq, void* ptr);
        static int hamlib_mode_cb(RIG* rig, vfo_t currVFO, rmode_t currMode, pbwidth_t passband, void* ptr);

        void update_sideband_status();

        RIG *m_rig;
        /* Sorted list of rigs. */
        riglist_t m_rigList;

        /* Sideband detection status box and state. */
        wxStaticText* m_sidebandBox;
        freq_t m_currFreq;
        rmode_t m_currMode;
};

#endif /*HAMLIB_H*/
