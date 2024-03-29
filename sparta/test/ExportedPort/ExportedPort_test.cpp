
#include "sparta/ports/ExportedPort.hpp"

#include "sparta/simulation/Clock.hpp"
#include "sparta/simulation/ClockManager.hpp"
#include "sparta/simulation/Unit.hpp"
#include "sparta/simulation/DynamicResourceTreeNode.hpp"

#include "sparta/kernel/Scheduler.hpp"
#include "sparta/utils/SpartaTester.hpp"
#include "sparta/ports/PortSet.hpp"
#include "sparta/ports/DataPort.hpp"
#include "sparta/ports/SyncPort.hpp"
#include "sparta/ports/SignalPort.hpp"
#include "sparta/ports/ExportedPort.hpp"
#include "sparta/events/StartupEvent.hpp"

#include <iostream>

class Unit1 : public sparta::Unit
{
public:
    Unit1(sparta::TreeNode * my_node) :
        sparta::Unit(my_node),
        sub_unit_params_(my_node),
        dyn_rtn_(my_node, "subunit", "Subunit in Unit1", &sub_unit_params_),
        exported_port_(getPortSet(), "a_signal_out_port",
                       my_node,      "a_deep_signal_out_port"),
        exported_port_same_name_(getPortSet(), "a_deep_signal_out_port",
                                 my_node,      "a_deep_signal_out_port"),
        exported_port_bad_(getPortSet(), "a_non_existant_port",
                           my_node,      "a_non_existant_port")
    {
    }

private:

    class SubUnit1 : public sparta::Unit
    {
    public:
        SubUnit1(sparta::TreeNode * my_node, const sparta::ParameterSet *) :
            sparta::Unit(my_node)
        {
            sparta::StartupEvent(my_node, CREATE_SPARTA_HANDLER(SubUnit1, writer_));
            a_signal_out_port_.participateInAutoPrecedence(false);
        }
    private:
        void writer_() {
            a_signal_out_port_.send(count_++);
            drive_.schedule();
        }
        int count_ = 1;
        sparta::Event<> drive_{getEventSet(), "drive", CREATE_SPARTA_HANDLER(SubUnit1, writer_), 1};
        sparta::DataOutPort<int> a_signal_out_port_{getPortSet(), "a_deep_signal_out_port"};
    };
    sparta::ParameterSet sub_unit_params_;
    sparta::DynamicResourceTreeNode<SubUnit1, sparta::ParameterSet> dyn_rtn_;

    sparta::ExportedPort exported_port_;
    sparta::ExportedPort exported_port_same_name_;
    sparta::ExportedPort exported_port_bad_;
};

class Unit2 : public sparta::Unit
{
public:
    Unit2(sparta::TreeNode * my_node):
        sparta::Unit(my_node)
    {
        a_signal_in_port_.registerConsumerHandler(CREATE_SPARTA_HANDLER_WITH_DATA(Unit2, reader, int));
    }

    ~Unit2() { std::cout << "Last payload: " << last_payload_ << std::endl; }

    void reader(const int & payload) {
        //std::cout << __PRETTY_FUNCTION__ << ": received signal" << std::endl;
        last_payload_ = payload;
    }

private:
    sparta::DataInPort<int> a_signal_in_port_{getPortSet(), "a_signal_in_port"};
    sparta::ExportedPort a_signal_in_port_exported_{getPortSet(), "a_signal_in_port_exported", &a_signal_in_port_};

    int last_payload_ = 0;
};


int main()
{
    sparta::Scheduler     sched;
    sparta::RootTreeNode  root;
    sparta::ClockManager  cm(&sched);
    sparta::Clock::Handle root_clk;
    root_clk = cm.makeRoot(&root, "root_clk");
    cm.normalize();
    root.setClock(root_clk.get());

    sparta::TreeNode unit1_tn(&root, "unit1", "unit 1");
    sparta::TreeNode unit2_tn(&root, "unit2", "unit 2");

    Unit1 unit1(&unit1_tn);
    Unit2 unit2(&unit2_tn);

    root.enterConfiguring();
    std::cout << root.renderSubtree() << std::endl;

    root.enterFinalized();
    std::cout << root.renderSubtree() << std::endl;

    auto exported_port_out =
        unit1.getPortSet()->getChildAs<sparta::Port>("a_signal_out_port");
    auto exported_port_in  =
        unit2.getPortSet()->getChildAs<sparta::Port>("a_signal_in_port_exported");

    EXPECT_FALSE(exported_port_out->isBound());
    EXPECT_FALSE(exported_port_in->isBound());

    EXPECT_EQUAL(exported_port_in->getDirection(), sparta::Port::Direction::IN);
    EXPECT_EQUAL(exported_port_out->getDirection(), sparta::Port::Direction::UNKNOWN);

    sparta::bind(unit1.getPortSet()->getChildAs<sparta::Port>("a_signal_out_port"),
                 unit2.getPortSet()->getChildAs<sparta::Port>("a_signal_in_port_exported"));
    bool caught_bad_bind = false;
    try {
        sparta::bind(unit1.getPortSet()->getChildAs<sparta::Port>("a_non_existant_port"),
                     unit2.getPortSet()->getChildAs<sparta::Port>("a_signal_in_port_exported"));
    }
    catch(...) {
        caught_bad_bind = true;
    }
    EXPECT_TRUE(caught_bad_bind);

    EXPECT_TRUE(exported_port_out->isBound());
    EXPECT_TRUE(exported_port_in->isBound());
    EXPECT_EQUAL(exported_port_in->getDirection(), sparta::Port::Direction::IN);
    EXPECT_EQUAL(exported_port_out->getDirection(), sparta::Port::Direction::OUT);

    EXPECT_TRUE(exported_port_in->doesParticipateInAutoPrecedence());
    EXPECT_FALSE(exported_port_out->doesParticipateInAutoPrecedence());

    sched.finalize();

    std::cout << root.renderSubtree() << std::endl;

    sched.run(20000000);

    root.enterTeardown();

    // Returns error if one
    REPORT_ERROR;
    return ERROR_CODE;
}
