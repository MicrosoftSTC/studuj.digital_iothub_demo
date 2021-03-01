SELECT 
        CAST(waterLevel AS float) AS waterLevel
INTO
        outputpowerbi
FROM 
        inputiothub
