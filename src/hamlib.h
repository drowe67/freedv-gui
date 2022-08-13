#ifndef HAMLIB_H
#define HAMLIB_H

#include <wx/stattext.h>
#include <wx/combobox.h>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>

extern "C" {
#include <hamlib/rig.h>
}

class Hamlib {

    public:
        Hamlib();
        ~Hamlib();
        
        // Name to index lookup and vice versa.
        unsigned int rigNameToIndex(std::string rigName);
        std::string rigIndexToName(unsigned int rigIndex);
        
        void populateComboBox(wxComboBox *cb);
        bool connect(unsigned int rig_index, const char *serial_port, const int serial_rate, const int civ_hex = 0);
        bool ptt(bool press, wxString &hamlibError);
        void enable_mode_detection(wxStaticText* statusBox, wxTextCtrl* freqBox, bool vhfUhfMode);
        void disable_mode_detection();
        void close(void);
        int get_serial_rate(void);
        int get_data_bits(void);
        int get_stop_bits(void);
        freq_t get_frequency(void) const;
        int update_frequency_and_mode(void);
        bool isActive() const { return m_rig != nullptr; }
        
        typedef std::vector<const struct rig_caps *> riglist_t;

    private:
        void update_mode_status();
        void statusUpdateThreadEntryFn_();
        void update_from_hamlib_();
        
        RIG *m_rig;
        rig_model_t m_rig_model;
        /* Sorted list of rigs. */
        riglist_t m_rigList;

        /* Mode detection status box and state. */
        wxStaticText* m_modeBox;
        wxTextCtrl* m_freqBox;
        freq_t m_currFreq;
        rmode_t m_currMode;
        bool m_vhfUhfMode;
        
        // Data elements to support running Hamlib operations in a separate thread.
        bool threadRunning_;
        std::thread statusUpdateThread_;
        std::condition_variable statusUpdateCV_;
        std::mutex statusUpdateMutex_;
        bool pttEnabled_;
};

#endif /*HAMLIB_H*/
