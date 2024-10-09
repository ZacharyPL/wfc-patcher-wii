#pragma once

#include "import/cxx.hpp"
#include "import/dwc.h"
#include "import/mkw/system/system.hpp"
#include "import/mkw/ui/multiMenuInputManager.hpp"
#include "import/mkw/ui/section/sectionManager.hpp"
#include "import/revolution.h"
#include "messagePopupPage.hpp"
#include "yesNoPopupPage.hpp"

namespace mkw::UI
{

#if RMC

class OpenHostPage : public Page
{
public:
    void onActivate() override
    {
        EGG::Heap* systemHeap = mkw::System::System::Instance().systemHeap();

        s_onOption =
            new (systemHeap, 4) MenuInputManager::Handler<OpenHostPage>(
                this, &OpenHostPage::onOption
            );
        MultiControlInputManager* multiControlInputManager =
            reinterpret_cast<MultiControlInputManager*>(menuInputManager());
        multiControlInputManager->setHandler(
            MenuInputManager::InputType::Option, s_onOption
        );

        s_onYesOrNo = new (systemHeap, 4)
            YesNoPage::Handler<OpenHostPage>(this, &OpenHostPage::onYesOrNo);
    }

    void onDeactivate() override
    {
        MultiControlInputManager* multiControlInputManager =
            reinterpret_cast<MultiControlInputManager*>(menuInputManager());
        multiControlInputManager->setHandler(
            MenuInputManager::InputType::Option, nullptr
        );
        delete s_onOption;
        s_onOption = nullptr;

        delete s_onYesOrNo;
        s_onYesOrNo = nullptr;
    }

    void onRefocus() override
    {
        transition(resolve());
    }

private:
    OpenHostPage();

    enum class State {
        Previous,
        Prompt,
        Result,
    };

    State resolve() const
    {
        switch (s_state) {
        case State::Previous: {
            break;
        }
        case State::Prompt: {
            return State::Result;
        }
        case State::Result: {
            return State::Previous;
        }
        }

        return s_state;
    }

    void transition(State state)
    {
        Section* section = SectionManager::Instance()->currentSection();
        u8 language = RVL::SCGetLanguage();

        if (state == s_state) {
            return;
        }

        switch (state) {
        case State::Previous: {
            break;
        }
        case State::Prompt: {
            FormatParam formatParam{};
	    if (language == 0) {
                formatParam.strings[0] =
                   L"こうかいホストをゆうこうにしますか？\n\n"
                   L"この機能はあなたのフレンドコードを\n"
                   L"追加したプレイヤーはあなたが追加し返さなくても\n"
                   L"フレンドとしてあなたと会えるようになる機能です";
	    } else if (language == 3) {
                formatParam.strings[0] =
                    L"Activer l'Open Host?\n\n"
                    L"Cette fonctionnalité permet aux joueurs qui\n"
                    L"ajoutent votre code ami de vous rejoindre,\n"
                    L"même si vous ne les avez pas ajoutés.";
	    } else {
                formatParam.strings[0] =
                    L"Enable Open Host?\n\n"
                    L"This feature allows players who\n"
                    L"add your friend code to meet up with you,\n"
                    L"even if you don't add them back.";
	    }
            YesNoPopupPage* yesNoPopupPage =
                section->page<YesNoPopupPage>(PageId::YesNoPopup);
            yesNoPopupPage->reset();
            yesNoPopupPage->setWindowMessage(0x19CA, &formatParam);
            yesNoPopupPage->configureButton(
                0, 0xFAC, nullptr, Animation::None, s_onYesOrNo
            );
            yesNoPopupPage->configureButton(
                1, 0xFAD, nullptr, Animation::None, s_onYesOrNo
            );
            yesNoPopupPage->setDefaultChoice(1);

            push(PageId::YesNoPopup, Animation::Next);
            break;
        }
        case State::Result: {
            FormatParam formatParam{};
            if (!s_sentOpenHostValue) {
                if (language == 0) {
                    formatParam.strings[0] = L"サーバーからの接続が切断されました\n\n\n"
                                             L"もう一度やり直してください";
		} else if (language == 3) {
                    formatParam.strings[0] = L"Vous avez perdu la connexion\n"
                                             L"au serveur.\n\n"
                                             L"Veuillez réessayer ultérieurement.";
		} else {
                    formatParam.strings[0] = L"You have lost connection to\n"
                                             L"the server.\n\n"
                                             L"Please try again later.";
		}
            } else {
                if (s_openHostEnabled) {
                    if (language == 0) {
                        formatParam.strings[0] = L"こうかいホストをゆうこうにしました！";
		    } else if (language == 3) {
                        formatParam.strings[0] = L"L'Open Host est maintenant activé!";
		    } else {
                        formatParam.strings[0] = L"Open Host is now enabled!";
		    }
                } else {
                    if (language == 0) {
                        formatParam.strings[0] = L"こうかいホストをむこうにしました！";
		    } else if (language == 3) {
                        formatParam.strings[0] = L"L'Open Host est maintenant désactivé!";
		    } else {
                        formatParam.strings[0] = L"Open Host is now disabled!";
		    }
                }
            }

            MessagePopupPage* messagePopupPage =
                section->page<MessagePopupPage>(PageId::MessagePopup);
            messagePopupPage->reset();
            messagePopupPage->setWindowMessage(0x19CA, &formatParam);

            push(PageId::MessagePopup, Animation::Next);
            break;
        }
        }

        s_state = state;
    }

    void onOption(u32 /* localPlayerId */)
    {
        transition(State::Prompt);
    }

    void onYesOrNo(int choice, void* /* pushButton */)
    {
        GameSpy::GPConnection* gpConnection = DWC::stpMatchCnt->connection;
        if (!gpConnection) {
            s_sentOpenHostValue = false;
            return;
        }
        s_sentOpenHostValue = true;

        bool openHostEnabled = choice == 0;

        char openHostValue[2];
        openHostValue[0] = '0' + openHostEnabled;
        openHostValue[1] = '\0';
        GameSpy::gpiSendLocalInfo(
            gpConnection, "\\wwfc_openhost\\", openHostValue
        );

        s_openHostEnabled = openHostEnabled;
    }

    static State s_state;
    static mkw::UI::MenuInputManager::Handler<OpenHostPage>* s_onOption;
    static mkw::UI::YesNoPage::Handler<OpenHostPage>* s_onYesOrNo;
    static bool s_openHostEnabled;
    static bool s_sentOpenHostValue;
};

static_assert(sizeof(OpenHostPage) == sizeof(Page));

#endif

} // namespace mkw::UI
