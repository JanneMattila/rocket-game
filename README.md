# RocketG ame

Rocket game

## Build image

```powershell
$resourceGroup = "rg-rocket"
$acrName = "rocket0000010"
$image = "rocketserver"
$location = "northeurope"

# Create resource group
az group create --name $resourceGroup --location $location

# Create ACR
az acr create --resource-group $resourceGroup --name $acrName --sku Basic

az acr update -n $acrName --admin-enabled true
$acrPassword=$(az acr credential show -n $acrName --query passwords[0].value -o tsv)

# Build image
cd src/cpp/RocketServer
docker build -t "$acrName.azurecr.io/$image" .

# Test image
docker run -it --rm -p 3501:3501/udp --name $image "$acrName.azurecr.io/$image"

# Login to ACR
az acr login --name $acrName

# Push image
docker push "$acrName.azurecr.io/$image"

# Deploy to ACI
az container create --resource-group $resourceGroup --name $image --image "$acrName.azurecr.io/$image" --cpu 1 --memory 1 --registry-login-server "$acrName.azurecr.io" --registry-username $acrName --registry-password $acrPassword --ports 3501 --protocol UDP --ip-address public --restart-policy OnFailure --os-type linux
```
