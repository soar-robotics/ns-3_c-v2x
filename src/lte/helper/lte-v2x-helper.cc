#include "lte-v2x-helper.h"
#include <ns3/double.h>
#include <ns3/log.h>
#include <ns3/abort.h>
#include <ns3/pointer.h>
#include <ns3/epc-helper.h>
#include <ns3/angles.h>
#include <ns3/random-variable-stream.h>
#include <iostream>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LteV2xHelper"); 

NS_OBJECT_ENSURE_REGISTERED (LteV2xHelper);

LteV2xHelper::LteV2xHelper (void)
{
    NS_LOG_FUNCTION(this);
}

LteV2xHelper::~LteV2xHelper(void)
{
    NS_LOG_FUNCTION(this);
}

TypeId 
LteV2xHelper::GetTypeId(void)
{   
    static TypeId
        tid = 
        TypeId ("ns3::LteV2xHelper")
        .SetParent<Object> ()
        .SetGroupName("Lte")
        .AddConstructor<LteV2xHelper> ()
    ;
    return tid;
}

void 
LteV2xHelper::DoDispose ()
{
    NS_LOG_FUNCTION (this);
    Object::DoDispose(); 
}

void 
LteV2xHelper::SetLteHelper(Ptr<LteHelper> h)
{
    NS_LOG_FUNCTION (this << h);
    m_lteHelper = h;
}

NetDeviceContainer 
LteV2xHelper::RemoveNetDevice (NetDeviceContainer container, Ptr<NetDevice> item)
{
  NetDeviceContainer newContainer;
  uint32_t nDevices = container.GetN ();
  for (uint32_t i = 0 ; i < nDevices; ++i)
    {
      Ptr<NetDevice> p = container.Get(i);
      if (item != p)
        {
          newContainer.Add (p);
        }
    }
  return newContainer;
}

std::vector<NetDeviceContainer>
LteV2xHelper::AssociateForBroadcast(double txPower, double ulEarfcn, double ulBandwidth, NetDeviceContainer ues, double rsrpThreshold, uint32_t ntransmitters, SrsrpMethod_t compMethod)
{
	std::vector < NetDeviceContainer > groups; //groups created
	NetDeviceContainer remainingUes; //list of UEs not assigned to groups
	remainingUes.Add (ues);
	// Start the selection of the transmitters
	NetDeviceContainer selectedTx;
	uint32_t numTransmittersSelected = 0;

	while (numTransmittersSelected < ntransmitters)
	  {
      //Transmitter UE is selected from all UEs within the entire 19 or 7 macro sites that are already not selected as transmitter or receiver UEs.

      Ptr<NetDevice> tx = ues.Get (numTransmittersSelected);
      NS_LOG_DEBUG (" Candidate Tx= " << tx->GetNode()->GetId());
      selectedTx.Add (tx);
      numTransmittersSelected++;
	  }

	  //For each remaining UE, associate to all transmitters where RSRP is greater than X dBm
	  for (uint32_t i = 0 ; i < numTransmittersSelected ; i++)
	    {
	      Ptr<NetDevice> tx = selectedTx.Get (i);
	      //prepare group for this transmitter
	      NetDeviceContainer newGroup (tx);
	      uint32_t nRxDevices = remainingUes.GetN ();
	      double rsrpRx = 0;
	      for (uint32_t j = 0 ; j < nRxDevices; ++j)
	        {
	          Ptr<NetDevice> rx = remainingUes.Get(j);
	          if(rx->GetNode()->GetId() != tx->GetNode()->GetId())//No loopback link possible due to half-duplex
	          {
	            if (compMethod == LteV2xHelper::SRSRP_STD)
	              {
	                rsrpRx = m_lteHelper->CalcSidelinkRsrp (txPower, ulEarfcn, ulBandwidth, tx, rx);
	              }
              else
	              {
	                rsrpRx = m_lteHelper->CalcSidelinkRsrpEval (txPower, ulBandwidth, tx, rx);
	              }
	              //If receiver UE is not within RSRP* of X dBm of the transmitter UE then randomly reselect the receiver UE among the UEs that are within the RSRP of X dBm of the transmitter UE and are not part of a group already.
	            NS_LOG_DEBUG ("\tCandidate Rx= " << rx->GetNode()->GetId() << " Rsrp=" << rsrpRx << " required=" << rsrpThreshold);
	            if (rsrpRx >= rsrpThreshold)
	              {
	                //good receiver
	                NS_LOG_DEBUG ("\tAdding Rx to group");
	                newGroup.Add (rx);
	              }
	          }
	        }
	      groups.push_back (newGroup);
	    }
	return groups;
}

std::vector < NetDeviceContainer > 
LteV2xHelper::AssociateForV2xBroadcast (NetDeviceContainer ues, uint32_t ntransmitters)
{
  std::vector < NetDeviceContainer > groups; //groups created

  for (uint32_t i = 0 ; i < ntransmitters ; i++)
    {
      Ptr<NetDevice> tx = ues.Get (i);

      NetDeviceContainer remainingUes; //list of UEs not assigned to groups
      remainingUes.Add (ues);
      remainingUes = RemoveNetDevice (remainingUes, tx); 
      //prepare group for this transmitter
      NetDeviceContainer newGroup (tx);
      uint32_t nRxDevices = remainingUes.GetN ();

      for (uint32_t j = 0 ; j < nRxDevices; j++)
        {
          Ptr<NetDevice> rx = remainingUes.Get(j);
          //good receiver              
          NS_LOG_DEBUG ("\tAdding Rx to group");
          newGroup.Add (rx);
        }
      groups.push_back (newGroup);
    }
  return groups;
}

void 
LteV2xHelper::PrintGroups (std::vector<NetDeviceContainer> groups)
{
    std::vector<NetDeviceContainer>::iterator gIt;
    for (gIt = groups.begin() ; gIt != groups.end() ; gIt++)
      {        
        std::cout << "Tx=" << (*gIt).Get (0)->GetNode()->GetId() << " Rx=";
        for (uint32_t i = 1; i < (*gIt).GetN(); i++)
          {
            std::cout << (*gIt).Get (i)->GetNode()->GetId() << " ";
          } 
        std::cout << std::endl;
      }
}

void
LteV2xHelper::PrintGroups (std::vector < NetDeviceContainer > groups, Ptr< OutputStreamWrapper > stream)
{
  //*stream->GetStream () << "TxNID\tRxNID" << std::endl;
  *stream->GetStream () << "TxNID\tRxNID\tTxIMSI\tRxIMSI" << std::endl;
  std::vector < NetDeviceContainer >::iterator gIt;
    for (gIt = groups.begin() ; gIt != groups.end() ; gIt++)
      {        
        if ((*gIt).GetN() < 2)
		{//No receiveres in group!
			*stream->GetStream () << (*gIt).Get (0)->GetNode()->GetId() << "\t0" << std::endl;
		}
		else
		{
		  for (uint32_t i = 1; i < (*gIt).GetN(); i++)
			{
			  //*stream->GetStream () << (*gIt).Get (0)->GetNode()->GetId() << "\t" << (*gIt).Get (i)->GetNode()->GetId() << std::endl;
			  *stream->GetStream () << (*gIt).Get (0)->GetNode()->GetId() << "\t" << (*gIt).Get (i)->GetNode()->GetId() << "\t" << (*gIt).Get(0)->GetObject<LteUeNetDevice> ()->GetImsi () << "\t" << (*gIt).Get(i)->GetObject<LteUeNetDevice> ()->GetImsi () <<std::endl;
			} 
        }
      }
}

void 
LteV2xHelper::ActivateSidelinkBearer (Time activationTime, NetDeviceContainer ues, Ptr<LteSlTft> tft)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (m_lteHelper, "Sidelink activation requires LteHelper to be registered with the LteV2xHelper");
  Simulator::Schedule (activationTime, &LteV2xHelper::DoActivateSidelinkBearer, this, ues, tft);
}

void 
LteV2xHelper::DoActivateSidelinkBearer (NetDeviceContainer ues, Ptr<LteSlTft> tft)
{
  NS_LOG_FUNCTION (this);
  m_lteHelper->ActivateSidelinkBearer (ues, tft);
}

void
LteV2xHelper::EnableTraces (void)
{
  EnablePhyTraces ();
  EnableMacTraces ();
  EnableRlcTraces ();
  EnablePdcpTraces ();
}

void
LteV2xHelper::EnableRlcTraces (void)
{
  NS_ASSERT_MSG (m_rlcStats == 0, "please make sure that LteHelper::EnableRlcTraces is called at most once");
  m_rlcStats = CreateObject<RadioBearerStatsCalculator> ("RLC");
  m_radioBearerStatsConnector.EnableRlcStats (m_rlcStats);
}


void
LteV2xHelper::EnablePhyTraces (void)
{
  EnableDlPhyTraces ();
  EnableUlPhyTraces ();
  EnableSlPhyTraces ();
  EnableDlTxPhyTraces ();
  EnableUlTxPhyTraces ();
  EnableSlTxPhyTraces ();
  EnableDlRxPhyTraces ();
  EnableUlRxPhyTraces ();
  EnableSlRxPhyTraces ();
  EnableSlPscchRxPhyTraces ();
}

void
LteV2xHelper::EnableDlTxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbPhy/DlPhyTransmission",
                   MakeBoundCallback (&PhyTxStatsCalculator::DlPhyTransmissionCallback, m_phyTxStats));
}

void
LteV2xHelper::EnableUlTxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/LteUePhy/UlPhyTransmission",
                   MakeBoundCallback (&PhyTxStatsCalculator::UlPhyTransmissionCallback, m_phyTxStats));
}

void
LteV2xHelper::EnableSlTxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/LteUePhy/SlPhyTransmission",
                   MakeBoundCallback (&PhyTxStatsCalculator::SlPhyTransmissionCallback, m_phyTxStats));
}

void
LteV2xHelper::EnableDlRxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/LteUePhy/DlSpectrumPhy/DlPhyReception",
                   MakeBoundCallback (&PhyRxStatsCalculator::DlPhyReceptionCallback, m_phyRxStats));
}

void
LteV2xHelper::EnableUlRxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbPhy/UlSpectrumPhy/UlPhyReception",
                   MakeBoundCallback (&PhyRxStatsCalculator::UlPhyReceptionCallback, m_phyRxStats));
}

void
LteV2xHelper::EnableSlRxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/LteUePhy/SlSpectrumPhy/SlPhyReception",
                   MakeBoundCallback (&PhyRxStatsCalculator::SlPhyReceptionCallback, m_phyRxStats));
}

void
LteV2xHelper::EnableSlPscchRxPhyTraces (void)
{
  Config::Connect ("/NodeList/*/DeviceList/*/LteUePhy/SlSpectrumPhy/SlPscchReception",
                   MakeBoundCallback (&PhyRxStatsCalculator::SlPscchReceptionCallback, m_phyRxStats));
}

void
LteV2xHelper::EnableMacTraces (void)
{
  EnableDlMacTraces ();
  EnableUlMacTraces ();
  EnableSlUeMacTraces ();
  EnableSlSchUeMacTraces ();
}

void
LteV2xHelper::EnableSlUeMacTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeMac/SlUeScheduling",
                   MakeBoundCallback (&MacStatsCalculator::SlUeSchedulingCallback, m_macStats));
}

void
LteV2xHelper::EnableSlSchUeMacTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeMac/SlSharedChUeScheduling",
                   MakeBoundCallback (&MacStatsCalculator::SlSharedChUeSchedulingCallback, m_macStats));
}

void
LteV2xHelper::EnableDlMacTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbMac/DlScheduling",
                   MakeBoundCallback (&MacStatsCalculator::DlSchedulingCallback, m_macStats));
}

void
LteV2xHelper::EnableUlMacTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbMac/UlScheduling",
                   MakeBoundCallback (&MacStatsCalculator::UlSchedulingCallback, m_macStats));
}

void
LteV2xHelper::EnableDlPhyTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/LteUePhy/ReportCurrentCellRsrpSinr",
                   MakeBoundCallback (&PhyStatsCalculator::ReportCurrentCellRsrpSinrCallback, m_phyStats));
}

void
LteV2xHelper::EnableUlPhyTraces (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbPhy/ReportUeSinr",
                   MakeBoundCallback (&PhyStatsCalculator::ReportUeSinr, m_phyStats));
  Config::Connect ("/NodeList/*/DeviceList/*/ComponentCarrierMap/*/LteEnbPhy/ReportInterference",
                   MakeBoundCallback (&PhyStatsCalculator::ReportInterference, m_phyStats));
}

void
LteV2xHelper::EnableSlPhyTraces (void)
{
  //TBD
}

void
LteV2xHelper::EnablePdcpTraces (void)
{
  NS_ASSERT_MSG (m_pdcpStats == 0, "please make sure that LteHelper::EnablePdcpTraces is called at most once");
  m_pdcpStats = CreateObject<RadioBearerStatsCalculator> ("PDCP");
  m_radioBearerStatsConnector.EnablePdcpStats (m_pdcpStats);
}

void
LteV2xHelper::EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename)
{
  //
  // All of the Pcap enable functions vector through here including the ones
  // that are wandering through all of devices on perhaps all of the nodes in
  // the system.  We can only deal with devices of type PointToPointNetDevice.
  //
  Ptr<NetDevice> device = nd->GetObject<NetDevice> ();
  if (device == 0)
    {
      NS_LOG_INFO ("NetDevice::EnablePcapInternal(): Device " << device << " not of type ns3::PointToPointNetDevice");
      return;
    }

  PcapHelper pcapHelper;

  std::string filename;
  if (explicitFilename)
    {
      filename = prefix;
    }
  else
    {
      filename = pcapHelper.GetFilenameFromDevice (prefix, device);
    }

  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (filename, std::ios::out,
                                                     PcapHelper::DLT_PPP);
  pcapHelper.HookDefaultSink<NetDevice> (device, "PromiscSniffer", file);
}

void
LteV2xHelper::EnableAsciiInternal (
  Ptr<OutputStreamWrapper> stream,
  std::string prefix,
  Ptr<NetDevice> nd,
  bool explicitFilename)
{
  //
  // All of the ascii enable functions vector through here including the ones
  // that are wandering through all of devices on perhaps all of the nodes in
  // the system.  We can only deal with devices of type PointToPointNetDevice.
  //
  Ptr<NetDeviceQueue> device = nd->GetObject<NetDevice> ();
  if (device == 0)
    {
      NS_LOG_INFO ("PointToPointHelper::EnableAsciiInternal(): Device " << device <<
                   " not of type ns3::PointToPointNetDevice");
      return;
    }

  //
  // Our default trace sinks are going to use packet printing, so we have to
  // make sure that is turned on.
  //
  Packet::EnablePrinting ();

  //
  // If we are not provided an OutputStreamWrapper, we are expected to create
  // one using the usual trace filename conventions and do a Hook*WithoutContext
  // since there will be one file per context and therefore the context would
  // be redundant.
  //
  if (stream == 0)
    {
      //
      // Set up an output stream object to deal with private ofstream copy
      // constructor and lifetime issues.  Let the helper decide the actual
      // name of the file given the prefix.
      //
      AsciiTraceHelper asciiTraceHelper;

      std::string filename;
      if (explicitFilename)
        {
          filename = prefix;
        }
      else
        {
          filename = asciiTraceHelper.GetFilenameFromDevice (prefix, device);
        }

      Ptr<OutputStreamWrapper> theStream = asciiTraceHelper.CreateFileStream (filename);

      //
      // The MacRx trace source provides our "r" event.
      //
      asciiTraceHelper.HookDefaultReceiveSinkWithoutContext<NetDevice> (device, "MacRx", theStream);

      //
      // The "+", '-', and 'd' events are driven by trace sources actually in the
      // transmit queue.
      //
      Ptr<Queue<Packet> > queue = device->GetQueueLimits ();
      asciiTraceHelper.HookDefaultEnqueueSinkWithoutContext<Queue<Packet> > (queue, "Enqueue", theStream);
      asciiTraceHelper.HookDefaultDropSinkWithoutContext<Queue<Packet> > (queue, "Drop", theStream);
      asciiTraceHelper.HookDefaultDequeueSinkWithoutContext<Queue<Packet> > (queue, "Dequeue", theStream);

      // PhyRxDrop trace source for "d" event
      asciiTraceHelper.HookDefaultDropSinkWithoutContext<NetDevice> (device, "PhyRxDrop", theStream);

      return;
    }

  //
  // If we are provided an OutputStreamWrapper, we are expected to use it, and
  // to providd a context.  We are free to come up with our own context if we
  // want, and use the AsciiTraceHelper Hook*WithContext functions, but for
  // compatibility and simplicity, we just use Config::Connect and let it deal
  // with the context.
  //
  // Note that we are going to use the default trace sinks provided by the
  // ascii trace helper.  There is actually no AsciiTraceHelper in sight here,
  // but the default trace sinks are actually publicly available static
  // functions that are always there waiting for just such a case.
  //
  uint32_t nodeid = nd->GetNode ()->GetId ();
  uint32_t deviceid = nd->GetIfIndex ();
  std::ostringstream oss;

  oss << "/NodeList/" << nd->GetNode ()->GetId () << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/MacRx";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultReceiveSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Enqueue";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Dequeue";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDequeueSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/TxQueue/Drop";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDropSinkWithContext, stream));

  oss.str ("");
  oss << "/NodeList/" << nodeid << "/DeviceList/" << deviceid << "/$ns3::PointToPointNetDevice/PhyRxDrop";
  Config::Connect (oss.str (), MakeBoundCallback (&AsciiTraceHelper::DefaultDropSinkWithContext, stream));
}






}