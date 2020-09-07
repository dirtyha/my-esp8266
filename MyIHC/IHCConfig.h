#ifndef IHCCONFIG_H
#define IHCCONFIG_H

#include <IHCIO.h>

#define MAX_MODULES 16
#define MAX_PORTS 8
#define SIZE sizeof(ios)/sizeof(ios[0])

byte vectorStorage[MAX_MODULES];
Vector<byte> data;

IHCIO kitchenStove("hella", 1, 1);
IHCIO porch("kuisti", 1, 2);
IHCIO entry("tuulikaappi", 1, 3);
IHCIO boiler("KV varaaja", 1, 4);
IHCIO hall("AK halli", 1, 5);
IHCIO bay("erkkeri", 1, 6);
IHCIO hallCeilingSocket("AK hallin KPR", 1, 8);
IHCIO UtilityRoom("KHH", 2, 1);
IHCIO masterBathroom("AK WC", 2, 2);
IHCIO kitchenCabinets("keittion kaapit", 2, 3);
IHCIO kitchen("keittio", 2, 4);
IHCIO kitchenCeilingSocket("keittio KPR", 2, 5);
IHCIO livingroomCeilingSocket("OH KPR", 2, 6);
IHCIO socketsDownstairs("AK PR", 3, 2);
IHCIO stairs("portaat", 3, 3);
IHCIO terrace("terassi", 3, 6);
IHCIO smallBedroom("pieni lasten MH", 3, 7);
IHCIO door("ovi", 3, 8);
IHCIO officeCeilingSocket("TH KPR", 4, 1);
IHCIO office("TH", 4, 3);
IHCIO saunaFiber("saunan kuitu", 4, 4);
IHCIO washingRoom("PH", 4, 5);
IHCIO saunaWall("saunan lauteet", 4, 6);
IHCIO smallBedroomCeilingSocket("pieni lasten MH KPR", 4, 7);
IHCIO livingroomDimmer("OH himmennin", 5, 1);
IHCIO bigBedroomCeilingSocket("iso lasten MH KPR", 5, 2);
IHCIO hallUpstairs("YK halli", 5, 4);
IHCIO hallUpstairsCeilingSocket("YK halli KPR", 5, 5);
IHCIO masterBedroom("iso MH", 5, 7);
IHCIO masterBedroomWall("iso MH seina", 6, 1);
IHCIO bigBedroom("iso lasten MH", 6, 3);
IHCIO bathroomCabinet("YK WC peili", 6, 4);
IHCIO bathroom("YK WC", 6, 5);
IHCIO socketsUpstairs("YK PR", 6, 7);
IHCIO yard("piha", 7, 1);
IHCIO outerWall("ulkoseina", 7, 2);
IHCIO home("kotona", 7, 3);
IHCIO alarm("halytys", 7, 4);
IHCIO security("haly viritetty", 8, 2);
IHCIO program("ohjelmointi", 8, 4);

IHCIO ios[] = {
  kitchenStove,
  porch,
  entry,
  boiler,
  hall,
  bay,
  hallCeilingSocket,
  UtilityRoom,
  masterBathroom,
  kitchenCabinets,
  kitchen,
  kitchenCeilingSocket,
  livingroomCeilingSocket,
  socketsDownstairs,
  stairs,
  terrace,
  smallBedroom,
  door,
  officeCeilingSocket,
  office,
  saunaFiber,
  washingRoom,
  saunaWall,
  smallBedroomCeilingSocket,
  livingroomDimmer,
  bigBedroomCeilingSocket,
  hallUpstairs,
  hallUpstairsCeilingSocket,
  masterBedroom,
  masterBedroomWall,
  bigBedroom,
  bathroomCabinet,
  bathroom,
  socketsUpstairs,
  yard,
  outerWall,
  home,
  alarm,
  security,
  program
};

IHCIO *findIO(unsigned short module, unsigned short port)
{
  for (int i = 0; i < SIZE; i++)
  {
    if (ios[i].getModule() == module && ios[i].getPort() == port)
    {
      return &ios[i];
    }
  }

  return NULL;
};

unsigned long updateStates(Vector<byte> *pNewData)
{
  unsigned long ret = 0;
  unsigned long changeId = millis();
  if (pNewData != NULL) {
    for (unsigned short mi = 0; mi < pNewData->size(); mi++)
    {
      byte moduleByte = (*pNewData)[mi];
      for (unsigned short pi = 0; pi < MAX_PORTS; pi++)
      {
        bool state = moduleByte & (0x1 << pi);
        IHCIO *io = findIO(mi + 1, pi + 1);
        if (io != NULL)
        {
          if (io->setState(state, changeId)) {
            ret = changeId;
          }
        }
      }
    }
  }

  return ret;
}

Vector<byte> *changeOutput(unsigned short module, unsigned short port, bool state) {

  data.setStorage(vectorStorage);
  data.clear();

  data.push_back((module - 1) * 10 + port);
  data.push_back(state ? 0x01 : 0x00);

  return &data;
}

#endif /* IHCCONFIG_H */
